#include "Gesture.h"
#include <Brush/BrushPreset.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace ii::brush {
namespace {
bool validInput(const StrokePoint& point)
{
    return std::isfinite(point.position.x) && std::abs(point.position.x)<=1e9
        && std::isfinite(point.position.y) && std::abs(point.position.y)<=1e9
        && std::isfinite(point.time) && std::abs(point.time)<=1e12
        && std::isfinite(point.pressure) && point.pressure>=0 && point.pressure<=1
        && std::isfinite(point.tiltX) && std::abs(point.tiltX)<=1
        && std::isfinite(point.tiltY) && std::abs(point.tiltY)<=1
        && std::isfinite(point.rotationRadians)
        && std::isfinite(point.tangentialPressure) && std::abs(point.tangentialPressure)<=1
        && std::isfinite(point.velocity) && point.velocity>=0
        && std::isfinite(point.arcLength) && point.arcLength>=0
        && std::ranges::all_of(point.custom,[](double v){return std::isfinite(v);});
}
void measure(PreparedGesture& gesture)
{
    gesture.distances.assign(gesture.points.size(),0);
    for(std::size_t i=1;i<gesture.points.size();++i) {
        const auto a=gesture.points[i-1].position,b=gesture.points[i].position;
        gesture.distances[i]=gesture.distances[i-1]+std::hypot(b.x-a.x,b.y-a.y);
    }
    gesture.length=gesture.distances.back();
}
struct Moment { double mass=0;DocumentPoint center{}; };
Moment merge(Moment a, Moment b)
{
    if(a.mass==0) return b;if(b.mass==0) return a;
    const auto w=b.mass/(a.mass+b.mass);
    return {a.mass+b.mass,{a.center.x+(b.center.x-a.center.x)*w,a.center.y+(b.center.y-a.center.y)*w}};
}
// Stable weighted centroids avoid subtraction of large accumulated moments.
// A range query costs O(log n), including densely sampled or stationary input.
class ArcAverage {
public:
    explicit ArcAverage(const PreparedGesture& gesture):g(gesture),nodes(gesture.points.size()*4)
    { build(1,0,g.points.size()-1); }
    Moment query(double low,double high) const { return query(1,0,g.points.size()-1,low,high); }
private:
    const PreparedGesture& g;
    std::vector<Moment> nodes;
    Moment build(std::size_t node,std::size_t begin,std::size_t end)
    {
        if(end==begin+1) {
            const auto a=g.points[begin].position,b=g.points[end].position;
            return nodes[node]={g.distances[end]-g.distances[begin],{a.x+(b.x-a.x)/2,a.y+(b.y-a.y)/2}};
        }
        const auto middle=(begin+end)/2;
        return nodes[node]=merge(build(node*2,begin,middle),build(node*2+1,middle,end));
    }
    Moment query(std::size_t node,std::size_t begin,std::size_t end,double low,double high) const
    {
        const auto start=g.distances[begin],finish=g.distances[end];
        if(finish<=low || start>=high || finish<=start) return {};
        if(low<=start && high>=finish) return nodes[node];
        if(end==begin+1) {
            const auto a=g.points[begin].position,b=g.points[end].position;
            const auto left=std::max(start,low),right=std::min(finish,high);
            const auto t=((left-start)+(right-start))/(2*(finish-start));
            return {right-left,{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}};
        }
        const auto middle=(begin+end)/2;
        return merge(query(node*2,begin,middle,low,high),query(node*2+1,middle,end,low,high));
    }
};
DevicePixelRect intersection(DevicePixelRect a,DevicePixelRect b)
{
    const auto x=std::max<std::int64_t>(a.origin.x,b.origin.x),y=std::max<std::int64_t>(a.origin.y,b.origin.y);
    const auto right=std::min(static_cast<std::int64_t>(a.origin.x)+a.width,static_cast<std::int64_t>(b.origin.x)+b.width);
    const auto bottom=std::min(static_cast<std::int64_t>(a.origin.y)+a.height,static_cast<std::int64_t>(b.origin.y)+b.height);
    return {{static_cast<int>(x),static_cast<int>(y)},static_cast<int>(std::max<std::int64_t>(0,right-x)),static_cast<int>(std::max<std::int64_t>(0,bottom-y))};
}
DevicePixelRect united(DevicePixelRect a,DevicePixelRect b)
{
    if(a.width<=0 || a.height<=0) return b;
    const auto x=std::min(a.origin.x,b.origin.x),y=std::min(a.origin.y,b.origin.y);
    const auto right=std::max(a.origin.x+a.width,b.origin.x+b.width),bottom=std::max(a.origin.y+a.height,b.origin.y+b.height);
    return {{x,y},right-x,bottom-y};
}
std::uint32_t sourcePixel(const RasterSourceSampler& source,DevicePixelPoint point)
{
    if(!source.sampleArgb) return 0;
    const auto x=static_cast<std::int64_t>(point.x)-source.origin.x;
    const auto y=static_cast<std::int64_t>(point.y)-source.origin.y;
    if(x<0 || y<0 || x>=source.width || y>=source.height) return 0;
    return source.sampleArgb(source.context,{static_cast<int>(x),static_cast<int>(y)});
}
int tileCoordinate(int value) { const auto q=value/64;return value%64<0?q-1:q; }
struct TileStorage { DevicePixelRect bounds;RasterLayer original;StrokeCompositeBuffer pending; };
}

std::expected<PreparedGesture, Error> prepareGesture(std::span<const StrokePoint> inputs,const Controls& controls,const Limits& limits)
{
    if(inputs.empty()) return std::unexpected(Error{ErrorCode::InvalidInput,"gesture has no samples"});
    if(inputs.size()>limits.maxInputPoints) return std::unexpected(Error{ErrorCode::BudgetExceeded,"gesture exceeds input budget"});
    for(std::size_t i=0;i<inputs.size();++i)
        if(!validInput(inputs[i]) || (i>0 && inputs[i].time<inputs[i-1].time))
            return std::unexpected(Error{ErrorCode::InvalidInput,"gesture samples must be finite and time must be monotonic"});
    PreparedGesture result;result.points.assign(inputs.begin(),inputs.end());
    const auto amount=controls.get(Parameter::Stabilization)/100.;
    if(amount>0 && inputs.size()>2) {
        const double tau=.25*amount*amount;
        for(std::size_t i=1;i+1<inputs.size();++i) {
            const auto dt=inputs[i].time-inputs[i-1].time;
            const auto weight=-std::expm1(-dt/tau);
            const auto previous=result.points[i-1].position,current=inputs[i].position;
            result.points[i].position={previous.x+(current.x-previous.x)*weight,previous.y+(current.y-previous.y)*weight};
        }
    }
    measure(result);
    const auto radius=controls.get(Parameter::PostCorrection);
    if(radius>0 && result.length>0 && result.points.size()>2) {
        const ArcAverage average(result);
        auto corrected=result.points;
        for(std::size_t i=1;i+1<result.points.size();++i) {
            const auto s=result.distances[i];
            const auto h=std::min({radius,s,result.length-s});
            if(h<=1e-9) continue;
            const auto moment=average.query(s-h,s+h);
            if(moment.mass<=0) continue;
            auto position=moment.center;
            const auto original=result.points[i].position;
            const auto dx=position.x-original.x,dy=position.y-original.y;
            const auto distance=std::hypot(dx,dy);
            if(distance>h) position={original.x+dx*h/distance,original.y+dy*h/distance};
            corrected[i].position=position;
        }
        result.points=std::move(corrected);measure(result);
    }
    return result;
}

std::expected<RenderedStroke, Error> renderGesture(std::span<const StrokePoint> inputs,Tool tool,const Controls& controls,
    const RenderTarget& target,const Limits& limits)
{
    return renderGestureWithPreset(inputs,tool,controls,BrushPreset{},target,limits);
}
std::expected<RenderedStroke, Error> renderGestureWithPreset(std::span<const StrokePoint> inputs,
    Tool tool,const Controls& controls,const BrushPreset& base,const RenderTarget& target,const Limits& limits)
{
    const auto& clip=target.clip;
    if(clip.width<=0 || clip.height<=0 || std::abs(static_cast<std::int64_t>(clip.origin.x))>1000000000
        || std::abs(static_cast<std::int64_t>(clip.origin.y))>1000000000
        || static_cast<std::int64_t>(clip.origin.x)+clip.width>1000000000
        || static_cast<std::int64_t>(clip.origin.y)+clip.height>1000000000)
        return std::unexpected(Error{ErrorCode::InvalidInput,"render clip is outside the supported coordinate range"});
    if(target.source.sampleArgb && (target.source.width<=0 || target.source.height<=0))
        return std::unexpected(Error{ErrorCode::InvalidInput,"a source sampler requires positive bounds"});
    auto brush=resolveWithPreset(tool,controls,base,target.argb,target.seed,target.dynamics);
    if(!brush) return std::unexpected(brush.error());
    auto gesture=prepareGesture(inputs,controls,limits);
    if(!gesture) return std::unexpected(gesture.error());
    std::map<std::pair<int,int>,TileStorage> tiles;
    std::size_t projected=0,touched=0;
    RenderedStroke result;result.length=gesture->length;
    RasterDabStream stream;
    auto sampler=target.source;
    if(!sampler.sampleArgb) sampler={nullptr,[](const void*,DevicePixelPoint){return std::uint32_t{0};},
        clip.origin,clip.width,clip.height};
    std::vector<RasterSample> current(1);
    try {
        for(std::size_t i=0;i<gesture->points.size();++i) {
            auto dabs=appendRasterDabs(stream,gesture->points[i],brush->state,i+1==gesture->points.size());
            if(dabs.size()>limits.maxDabs-result.dabCount)
                return std::unexpected(Error{ErrorCode::BudgetExceeded,"gesture exceeds dab budget"});
            result.dabCount+=dabs.size();
            for(auto& dab:dabs) {
                dab.scale*=taperSize(brush->taper,dab.strokeDistance,gesture->length);
                if(dab.scale<=0) continue;
                if(brush->hairline) dab.ellipseScaleY=1/std::max(1e-9,brush->state.rasterizer.brushSize*dab.scale);
                const auto bounds=deviceBoundsForBrushDab(dab,brush->state.rasterizer,{});
                const auto clipped=intersection(bounds,clip);
                if(clipped.width<=0 || clipped.height<=0) continue;
                const auto area=static_cast<std::uint64_t>(bounds.width)*bounds.height;
                if(area>limits.maxProjectedPixels-projected)
                    return std::unexpected(Error{ErrorCode::BudgetExceeded,"gesture exceeds projection budget"});
                projected+=static_cast<std::size_t>(area);
                auto samples=projectBrushDabs(BrushDabSpan{&dab,1},brush->state.rasterizer,{},sampler,brush->state.material);
                for(auto sample:samples) {
                    const auto world=sample.position;
                    if(world.x<clip.origin.x || world.y<clip.origin.y
                        || world.x>=clip.origin.x+clip.width || world.y>=clip.origin.y+clip.height) continue;
                    const auto column=tileCoordinate(world.x),row=tileCoordinate(world.y);
                    const auto key=std::pair{row,column};
                    auto found=tiles.find(key);
                    if(found==tiles.end()) {
                        const auto tileBounds=intersection({{column*64,row*64},64,64},clip);
                        const auto pixels=static_cast<std::size_t>(tileBounds.width)*tileBounds.height;
                        if(pixels>limits.maxTouchedPixels-touched)
                            return std::unexpected(Error{ErrorCode::BudgetExceeded,"gesture exceeds touched-pixel budget"});
                        touched+=pixels;
                        TileStorage tile{tileBounds,makeRasterLayer(tileBounds.width,tileBounds.height),
                            makeStrokeCompositeBuffer(tileBounds.width,tileBounds.height)};
                        for(int y=0;y<tileBounds.height;++y) for(int x=0;x<tileBounds.width;++x)
                            tile.original.pixels[static_cast<std::size_t>(y)*tileBounds.width+x]=sourcePixel(target.source,
                                {tileBounds.origin.x+x,tileBounds.origin.y+y});
                        found=tiles.emplace(key,std::move(tile)).first;
                    }
                    auto& tile=found->second;
                    sample.position={world.x-tile.bounds.origin.x,world.y-tile.bounds.origin.y};
                    sample.blendMode=RasterBlendMode::SourceOver;
                    // One current projected sample; no dab or replay list survives this iteration.
                    current[0]=sample;
                    accumulateStrokeSamples(tile.pending,current);
                }
            }
        }
    } catch(const std::length_error& error) {
        return std::unexpected(Error{ErrorCode::BudgetExceeded,error.what()});
    } catch(const std::invalid_argument& error) {
        return std::unexpected(Error{ErrorCode::InvalidInput,error.what()});
    }
    for(auto& [key,tile]:tiles) {
        (void)key;
        auto final=tile.original;
        if(brush->eraser) eraseStrokeBufferFromLayer(final,tile.pending);
        else compositeStrokeBufferOntoLayer(final,tile.pending);
        if(final.pixels!=tile.original.pixels) {
            result.dirty=united(result.dirty,tile.bounds);
            result.tiles.push_back({tile.bounds,std::move(final)});
        }
    }
    return result;
}
}
