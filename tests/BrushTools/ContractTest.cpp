#include <iiBrushTools/Gesture.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>

using namespace ii::brush;
using P = Parameter;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void set(Controls& controls, P key, double value) { check(controls.set(key, value), "valid control rejected"); }
Controls plain()
{
    Controls c;
    for (auto key : {P::Stabilization, P::PostCorrection, P::Pressure, P::Tilt, P::Velocity,
                     P::Scatter, P::Start, P::End}) set(c,key,0);
    set(c,P::Size,16); set(c,P::Hardness,100); set(c,P::Flow,100);
    set(c,P::Opacity,100); set(c,P::Spacing,5); set(c,P::Roundness,100);
    return c;
}
StrokePoint point(double x, double y, double time, double pressure=1)
{ StrokePoint p; p.position={x,y}; p.time=time; p.pressure=pressure; return p; }
std::vector<StrokePoint> line()
{ return {point(16,40,0),point(80,40,.5),point(144,40,1)}; }
std::uint32_t at(const RenderedStroke& image, int x, int y, std::uint32_t fallback=0)
{
    for (const auto& tile : image.tiles) {
        const auto& b=tile.bounds;
        if (x>=b.origin.x && y>=b.origin.y && x<b.origin.x+b.width && y<b.origin.y+b.height)
            return tile.pixels.pixels[static_cast<std::size_t>(y-b.origin.y)*b.width+x-b.origin.x];
    }
    return fallback;
}
RenderedStroke render(const Controls& c, Tool tool=Tool::Paint,
                     std::vector<StrokePoint> inputs=line(), RenderTarget target={{0,0,192,96}})
{
    auto r=renderGesture(inputs,tool,c,target);
    if (!r) throw std::runtime_error(r.error().message);
    return std::move(*r);
}
int width(const RenderedStroke& r, int x)
{ int n=0; for(int y=0;y<96;++y) if ((at(r,x,y)>>24)>=128) ++n; return n; }
bool same(const RenderedStroke& a, const RenderedStroke& b, const RasterSourceSampler& source={})
{
    for(int y=0;y<96;++y) for(int x=0;x<192;++x) {
        const auto sx=static_cast<std::int64_t>(x)-source.origin.x,sy=static_cast<std::int64_t>(y)-source.origin.y;
        const auto background=source.sampleArgb && sx>=0 && sy>=0 && sx<source.width && sy<source.height
            ?source.sampleArgb(source.context,{static_cast<int>(sx),static_cast<int>(sy)}):0;
        if(at(a,x,y,background)!=at(b,x,y,background)) return false;
    }
    return true;
}
void controlsTest()
{
    Controls c;
    check(parameterInfo().size()==22,"must expose all 22 controls");
    check(c.get(P::Size)==64 && c.get(P::End)==30,"authoring defaults changed");
    const auto before=c.serialize();
    check(!c.set(P::Size,std::numeric_limits<double>::quiet_NaN()),"NaN accepted");
    check(!c.set(P::BristleCount,1.5),"fractional bristle count accepted");
    check(c.serialize()==before,"rejected value changed controls");
    Controls other; check(other.restore(before) && other.serialize()==before,"controls round trip failed");
    check(!other.restore(before+"size=2\n") && other.serialize()==before,"duplicate restore is not atomic");
    check(!other.restore("iibrushtools.controls=2\n") && other.serialize()==before,"future controls accepted");
    Controls fractions;
    check(fractions.set(P::Size, 64.125) && fractions.set(P::Angle, -17.375), "fraction setup failed");
    struct Comma final : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
    const std::locale old = std::locale();
    std::locale::global(std::locale(old, new Comma));
    const auto encoded = fractions.serialize();
    Controls decoded;
    const bool restored = decoded.restore(encoded);
    std::locale::global(old);
    check(restored && decoded.get(P::Size)==64.125 && decoded.get(P::Angle)==-17.375, "locale-independent fractional round trip failed");
    check(encoded.find("size=64.125\n") != std::string::npos, "decimal point depends on process locale");
    auto malformed = encoded;
    malformed.replace(malformed.find("size=64.125"), std::string("size=64.125").size(), "size=+64.125");
    check(!decoded.restore(malformed) && decoded.serialize()==encoded, "invalid numeric input was accepted or modified state");
    check(c.set(P::Roundness,0),"Figma roundness zero is missing");
    check(!availability(Tool::Paint,P::Wetness,c).enabled,"dry paint offers wet controls");
    check(availability(Tool::Mixer,P::Wetness,c).enabled,"mixer wetness disabled");
    check(!availability(Tool::Paint,P::BristleCount,c).enabled,"ordinary paint offers bristle count");
    check(availability(Tool::Bristle,P::BristleCount,c).enabled,"bristle count disabled");
    check(!availability(Tool::Airbrush,P::Spacing,c).enabled,"time brush offers ineffective spatial spacing");
    auto recipe=resolve(Tool::Mixer,c); check(bool(recipe),"mixer resolve failed");
    check(std::abs(recipe->state.material.simulation.pickup-.12)<1e-10,"dilution pickup meaning lost");
    check(std::abs(recipe->state.material.simulation.deposit-.88)<1e-10,"dilution deposit meaning lost");
    check(recipe->state.rasterizer.warmupDistance==0,"size taper reuses alpha warmup");
    check(recipe->state.dynamics.velocityToOpacity==0 && recipe->state.dynamics.bindings.size()==1
        && recipe->state.dynamics.bindings[0].inputMaximum==1200,"velocity percentage uses unnormalized pixel speed");
    check(!resolve(Tool::Paint,c,0xff000000U,1,{0}),"zero velocity reference accepted");
    auto scaled=resolve(Tool::Paint,c,0xff000000U,1,{2400});
    check(bool(scaled) && scaled->state.dynamics.bindings[0].inputMaximum==2400,"caller velocity scale was ignored");
}
void taperTest()
{
    TaperProfile p{.25,.5,.25,1};
    check(std::abs(taperSize(p,0,100)-.25)<1e-12,"start must reach minimum size");
    check(std::abs(taperSize(p,100,100)-.25)<1e-12,"end must reach minimum size");
    check(std::abs(taperSize(p,25,100)-1)<1e-12,"start length is not a whole-stroke fraction");
    check(std::abs(taperSize(p,75,100)-.625)<1e-12,"end smoothstep distance is wrong");
    check(std::abs(taperSize(p,150,200)-.625)<1e-12,"taper ratio depends on brush diameter");
    check(taperSize(p,0,0)==1,"a spatially empty dot was erased by taper");
    auto invalid=p;invalid.strength=std::numeric_limits<double>::quiet_NaN();
    check(taperSize(invalid,50,100)==1,"invalid public profile leaked a NaN size");
    auto c=plain(); set(c,P::Start,25); set(c,P::End,30); set(c,P::MinimumSize,25); set(c,P::TaperStrength,100);
    auto image=render(c);
    check(width(image,80)>=15,"taper changed the full-size middle");
    check(width(image,140)>=3 && width(image,140)<=8,"End does not narrow actual geometry");
    check(width(image,20)>=3 && width(image,20)<=8,"Start does not narrow actual geometry");
    check((at(image,140,40)>>24)>=240,"size taper reduced center alpha instead of size");
    set(c,P::TaperStrength,0); auto untapered=render(c);
    check(width(untapered,140)>=15,"zero taper strength still narrows the tip");
    auto resolved=resolve(Tool::Paint,c);
    check(bool(resolved),"brush resolve failed");
    const auto stateResult=renderGestureWithResolvedBrush(line(),*resolved,c,{{0,0,192,96}});
    check(stateResult && same(*stateResult,untapered),"resolved brush render changed the tool result");
    resolved->state.rasterizer.blendMode=RasterBlendMode::Multiply;
    check(!renderGestureWithResolvedBrush(line(),*resolved,c,{{0,0,192,96}}),"unsupported state blend accepted");
    resolved->state.rasterizer.blendMode=RasterBlendMode::SourceOver;
    resolved->state.rasterizer.smoothingEnabled=true;resolved->state.rasterizer.smoothing=.5;
    const auto smoothed=renderGestureWithResolvedBrush(line(),*resolved,c,{{0,0,192,96}});
    RasterDabStream stream;
    const auto samples=line();
    for(std::size_t i=0;i<samples.size();++i) (void)appendRasterDabs(stream,samples[i],resolved->state,i+1==samples.size());
    check(smoothed && std::abs(smoothed->length-stream.traveledDistance)<1e-10 && smoothed->length<128,
          "fractional taper did not use smoothed stroke distance");

    set(c,P::TaperStrength,100); set(c,P::MinimumSize,100);
    check(same(render(c),untapered),"minimum size 100 must disable size reduction");
}
void correctionTest()
{
    auto c=plain(); std::vector<StrokePoint> raw{point(20,40,0),point(40,40,.2),point(50,60,.4),point(60,40,.6),point(90,40,1)};
    raw[2].pressure=.4;raw[2].tiltX=.6;raw[2].rotationRadians=.7;raw[2].custom[3]=.25;raw[2].deviceState=5;
    auto unchanged=prepareGesture(raw,c); check(bool(unchanged),"plain preparation failed");
    for(std::size_t i=0;i<raw.size();++i)
        check(unchanged->points[i].position.x==raw[i].position.x && unchanged->points[i].position.y==raw[i].position.y,"zero correction changed input geometry");
    set(c,P::PostCorrection,20); auto corrected=prepareGesture(raw,c);
    check(bool(corrected) && corrected->points.size()==raw.size(),"post correction lost sensor samples");
    check(corrected->points.front().position.x==20 && corrected->points.back().position.x==90
        && corrected->points.front().position.y==40 && corrected->points.back().position.y==40,"post correction moved endpoints");
    check(corrected->points[2].position.y<57 && corrected->points[2].position.y>40,"post correction does not use neighboring geometry");
    for(std::size_t i=0;i<raw.size();++i) {
        check(corrected->points[i].time==raw[i].time && corrected->points[i].pressure==raw[i].pressure,"post correction changed sensor time/pressure");
        check(corrected->points[i].tiltX==raw[i].tiltX && corrected->points[i].rotationRadians==raw[i].rotationRadians
            && corrected->points[i].custom[3]==raw[i].custom[3] && corrected->points[i].deviceState==raw[i].deviceState,"post correction lost rich sensor values");
        check(std::hypot(corrected->points[i].position.x-raw[i].position.x,corrected->points[i].position.y-raw[i].position.y)<=20.000001,"post correction exceeded distance bound");
    }
    auto dense=raw; dense.insert(dense.begin()+1,point(30,40,.1));
    auto split=prepareGesture(dense,c); check(bool(split),"subdivided preparation failed");
    check(std::hypot(split->points[3].position.x-corrected->points[2].position.x,split->points[3].position.y-corrected->points[2].position.y)<1e-9,"post correction depends on collinear input density");
    auto paused=raw;paused.insert(paused.begin()+3,raw[2]);auto duplicates=prepareGesture(paused,c);
    check(bool(duplicates) && std::abs(duplicates->points[3].position.y-corrected->points[2].position.y)<1e-9,"stationary duplicate changes the geometry average");
    const std::vector<StrokePoint> large{point(999999900,999999900,0),point(999999940,999999940,.5),point(999999980,999999900,1)};
    const auto largeResult=prepareGesture(large,c);
    check(bool(largeResult) && std::isfinite(largeResult->points[1].position.y)
        && std::hypot(largeResult->points[1].position.x-large[1].position.x,largeResult->points[1].position.y-large[1].position.y)<=20.000001,"large-coordinate correction is unstable");
    check(!same(render(c,Tool::Paint,raw),render(plain(),Tool::Paint,raw)),"post correction has no pixel effect");
    set(c,P::PostCorrection,0); set(c,P::Stabilization,80);
    check(!same(render(c,Tool::Paint,raw),render(plain(),Tool::Paint,raw)),"stabilization has no pixel effect");
}
std::uint32_t source(const void*, DevicePixelPoint p)
{ return ((p.x/8+p.y/8)%2)==0 ? 0xff2040d0U : 0xff20b080U; }
void parametersTest()
{
    std::size_t covered=0;
    for(const auto& info:parameterInfo()) {
        auto a=plain(); auto b=a; Tool tool=Tool::Paint;
        auto inputs=line(); RenderTarget target{{0,0,192,96}};
        const auto key=info.parameter;
        double first=0,last=100;
        switch(key) {
            case P::Size:first=8;last=32;break;
            case P::Hardness:first=10;last=100;set(a,P::Spacing,100);set(b,P::Spacing,100);break;
            case P::Opacity:first=25;last=100;break;
            case P::Flow:first=5;last=90;set(a,P::Spacing,80);set(b,P::Spacing,80);break;
            case P::Roundness:first=0;last=100;break;
            case P::Angle:first=0;last=90;set(a,P::Roundness,25);set(b,P::Roundness,25);break;
            case P::Spacing:first=5;last=100;break;
            case P::Scatter:first=0;last=80;break;
            case P::BristleCount:tool=Tool::Bristle;first=1;last=64;break;
            case P::Stabilization:case P::PostCorrection:
                first=0;last=key==P::PostCorrection?20:80;
                inputs={point(20,40,0),point(45,65,.1),point(70,30,.2),point(95,40,.3)};break;
            case P::Pressure:inputs={point(16,40,0,.25),point(144,40,1,.25)};break;
            case P::Tilt:for(auto& p:inputs) p.tiltX=.9;break;
            case P::Velocity:inputs={point(16,40,0),point(144,40,.01)};break;
            case P::Wetness:case P::Mix:case P::Load:case P::Dilution:
                tool=Tool::Mixer;target.argb=0xffff0000U;
                target.source={nullptr,source,{0,0},192,96};
                for(auto* c:{&a,&b}) {set(*c,P::Wetness,80);set(*c,P::Mix,75);set(*c,P::Load,80);set(*c,P::Dilution,65);set(*c,P::Spacing,80);set(*c,P::Flow,30);}
                first=key==P::Load?10:0;last=key==P::Load?90:100;break;
            case P::Start:case P::End:case P::MinimumSize:case P::TaperStrength:
                for(auto* c:{&a,&b}) {set(*c,P::Start,40);set(*c,P::End,40);set(*c,P::MinimumSize,10);set(*c,P::TaperStrength,100);}
                break;
            case P::Count:throw std::runtime_error("sentinel exposed as a parameter");
        }
        set(a,key,first);set(b,key,last);
        const auto x=render(a,tool,inputs,target), y=render(b,tool,inputs,target);
        if(same(x,y,target.source)) throw std::runtime_error(std::string("parameter has no pixel effect: ")+std::string(info.key));
        ++covered;
    }
    check(covered==22,"not all 22 effects checked");
    auto c=plain(); const auto image=render(c);
    for(auto key:{P::Wetness,P::Mix,P::Load,P::Dilution,P::BristleCount}) set(c,key,parameterInfo()[static_cast<std::size_t>(key)].maximum);
    check(same(image,render(c)),"inactive controls modify ordinary paint");
    auto bristleA=plain(),bristleB=plain();set(bristleA,P::BristleCount,32);set(bristleB,P::BristleCount,64);
    check(!same(render(bristleA,Tool::Bristle),render(bristleB,Tool::Bristle)),"bristle count saturates above 16");
    auto hairline=plain();set(hairline,P::Roundness,0);
    const auto hairlineWidth=width(render(hairline),80);
    check(hairlineWidth>=1 && hairlineWidth<=2,"zero roundness must produce a visible hairline");
}
void inputTest()
{
    auto c=plain();auto inputs=line();const auto before=inputs;
    inputs[1].time=-1;
    auto r=renderGesture(inputs,Tool::Paint,c,{{0,0,192,96}});
    check(!r && r.error().code==ErrorCode::InvalidInput,"backward time accepted");
    inputs=before; inputs[1].tiltY=std::numeric_limits<double>::infinity();
    check(!prepareGesture(inputs,c),"infinite sensor input accepted");
    inputs=before;Limits limit;limit.maxInputPoints=2;
    check(!renderGesture(inputs,Tool::Paint,c,{{0,0,192,96}},limit),"input budget ignored");
    limit={};limit.maxDabs=1;check(!renderGesture(before,Tool::Paint,c,{{0,0,192,96}},limit),"dab budget ignored");
    limit={};limit.maxProjectedPixels=1;check(!renderGesture(before,Tool::Paint,c,{{0,0,192,96}},limit),"projection budget ignored");
    limit={};limit.maxTouchedPixels=1;check(!renderGesture(before,Tool::Paint,c,{{0,0,192,96}},limit),"pixel budget ignored");
    check(!renderGesture(before,static_cast<Tool>(999),c,{{0,0,192,96}}),"invalid tool accepted");
    check(!renderGesture(before,Tool::Paint,c,{{0,0,-1,96}}),"negative clip accepted");
    check(!renderGesture(before,Tool::Smudge,c,{{0,0,192,96},{nullptr,source,{0,0},0,96}}),"invalid sampler bounds accepted");
    for(std::size_t i=0;i<before.size();++i) check(before[i].position.x==line()[i].position.x,"renderer mutated caller input");
}
void tilesTest()
{
    auto c=plain();auto coarse=render(c);
    std::vector<StrokePoint> dense;for(int i=0;i<=128;++i) dense.push_back(point(16+i,40,i/128.));
    check(same(coarse,render(c,Tool::Paint,dense)),"collinear input partitions change raster pixels");
    auto negative=render(c,Tool::Paint,{point(-80,0,0),point(48,0,1)},{{-96,-32,192,96}});
    check(at(negative,-64,0)!=0 && at(negative,0,0)!=0,"negative tile seam has a gap");
    check(coarse.tiles.size()<=6,"small gesture allocates full document tiles");
    RenderTarget target{{0,0,192,96},{nullptr,source,{0,0},192,96}};
    auto erased=render(c,Tool::Eraser,line(),target);
    check((at(erased,80,40,source(nullptr,{80,40}))>>24)==0,"eraser did not remove source alpha");
    check(at(erased,80,25,source(nullptr,{80,25}))==source(nullptr,{80,25}),"eraser touched pixels outside its tip");
    target.argb=0;
    const auto colorlessEraser=render(c,Tool::Eraser,line(),target);
    check((at(colorlessEraser,80,40,source(nullptr,{80,40}))>>24)==0,"eraser coverage depends on foreground color alpha");
    auto empty=render(c,Tool::Smudge,line());
    // Dilution=100 removes loaded pigment; a blank source must stay transparent.
    set(c,P::Dilution,100);auto unloaded=render(c,Tool::Smudge,line());
    for(const auto& tile:unloaded.tiles) for(auto pixel:tile.pixels.pixels) check(pixel==0,"unloaded smudge creates pigment");
    (void)empty;
    auto air=plain();set(air,P::Size,4);set(air,P::Flow,2);
    auto held=render(air,Tool::Airbrush,{point(40,40,0),point(40,40,1)});
    const auto expected=std::lround(255*(1-std::pow(250./255,31)));
    check(std::abs(static_cast<long>((at(held,40,40)>>24))-expected)<=1,"stationary time brush dose is wrong");
    auto speed=plain();set(speed,P::Velocity,10);
    auto fast=render(speed,Tool::Paint,{point(16,40,0),point(144,40,.001)});
    check(std::abs(static_cast<long>(at(fast,80,40)>>24)-230)<=1,"ten percent velocity response must retain ninety percent opacity at high speed");
    auto defaults=render(Controls{});
    check((at(defaults,140,40)>>24)>128,"default velocity response erases the moving stroke");
}
}
int main(int argc,char**argv)
{
    try {
        const std::string test=argc>1?argv[1]:"";
        if(test=="Controls") controlsTest(); else if(test=="Taper") taperTest();
        else if(test=="Correction") correctionTest(); else if(test=="Parameters") parametersTest();
        else if(test=="Input") inputTest(); else if(test=="Tiles") tilesTest();
        else throw std::runtime_error("unknown test case");
        std::cout<<"IIBRUSHTOOLS_OK "<<test<<'\n'; return 0;
    } catch(const std::exception&e) {std::cerr<<e.what()<<'\n';return 1;}
}
