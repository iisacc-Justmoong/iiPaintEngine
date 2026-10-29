// Red-first exploratory contract for accepting validated base presets.
#include <iiBrushTools/Gesture.h>
#include <Brush/BrushPreset.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
using namespace ii::brush;
namespace {
bool equal(const RenderedStroke &a, const RenderedStroke &b) {
    if (a.tiles.size() != b.tiles.size() || a.dabCount != b.dabCount || a.length != b.length) return false;
    for (std::size_t i = 0; i < a.tiles.size(); ++i) {
        const auto &x=a.tiles[i], &y=b.tiles[i];
        if (x.bounds.origin.x!=y.bounds.origin.x || x.bounds.origin.y!=y.bounds.origin.y ||
            x.bounds.width!=y.bounds.width || x.bounds.height!=y.bounds.height ||
            x.pixels.pixels!=y.pixels.pixels) return false;
    }
    return true;
}
std::uint64_t alpha(const RenderedStroke &stroke, bool rightOnly = false) {
    std::uint64_t result = 0;
    for (const auto &tile : stroke.tiles)
        for (int y=0; y<tile.bounds.height; ++y) for (int x=0; x<tile.bounds.width; ++x)
            if (!rightOnly || tile.bounds.origin.x+x >= 32)
                result += tile.pixels.pixels[std::size_t(y)*tile.bounds.width+x] >> 24;
    return result;
}
std::uint32_t source(const void *, DevicePixelPoint) { return 0xff845020; }
}
int main() {
    Controls c;
    for (const auto &[p,v] : std::array<std::pair<Parameter,double>, 9>{{
            {Parameter::Size,8},{Parameter::Hardness,100},{Parameter::Opacity,100},
            {Parameter::Flow,100},{Parameter::Pressure,0},{Parameter::Scatter,0},
            {Parameter::Stabilization,0},{Parameter::PostCorrection,0},{Parameter::Start,0}}})
        if (!c.set(p,v)) return 1;
    if (!c.set(Parameter::End,0)) return 1;
    std::array<StrokePoint,2> samples{};
    samples[0].position={10,16}; samples[1].position={20,16};
    samples[0].pressure=samples[1].pressure=.5;
    samples[0].time=1; samples[1].time=2;
    RenderTarget target; target.clip={{0,0},64,64}; target.argb=0xff2040d0;
    const BrushPreset empty;
    for (const auto tool : {Tool::Paint,Tool::Eraser}) {
        auto input=target;
        if(tool==Tool::Eraser) input.source={nullptr,source,{0,0},64,64};
        auto old=renderGesture(samples,tool,c,input);
        auto next=renderGestureWithPreset(samples,tool,c,empty,input);
        if(!old || !next || !equal(*old,*next)) { std::cerr << "default parity\n"; return 2; }
    }
    BrushPreset settings;
    settings.size=34; settings.stroke.spacing=3;
    settings.stroke.warmupDistance=9; settings.stroke.smoothingEnabled=true;
    settings.stroke.airbrushEnabled=true;
    settings.stroke.symmetry.mode=BrushSymmetryMode::Vertical;
    settings.stroke.symmetry.center={32,32};
    settings.dynamics.opacityResponse.enabled=true;
    settings.dynamics.opacityResponse.pressure={true,.25,.25,.25};
    settings.material.simulation.enabled=true;
    settings.material.bristle.enabled=true; settings.material.bristle.count=3;
    auto resolved=resolveWithPreset(Tool::Paint,c,settings,target.argb);
    if(!resolved || resolved->state.rasterizer.brushSize!=8 ||
       resolved->state.rasterizer.spacing!=3 || resolved->state.rasterizer.warmupDistance!=9 ||
       resolved->state.rasterizer.smoothingEnabled || resolved->state.rasterizer.airbrushEnabled ||
       resolved->state.rasterizer.symmetry.mode!=BrushSymmetryMode::Vertical ||
       !resolved->state.dynamics.opacityResponse.enabled ||
       !resolved->state.dynamics.opacityResponse.pressure.enabled ||
       resolved->state.material.simulation.enabled || resolved->state.material.bristle.enabled ||
       settings.size!=34 || settings.stroke.warmupDistance!=9 || !settings.stroke.smoothingEnabled) {
        std::cerr << "preset precedence or source mutation\n"; return 3;
    }
    BrushPreset symmetry;
    symmetry.stroke.symmetry.mode=BrushSymmetryMode::Vertical;
    symmetry.stroke.symmetry.center={32,32};
    const auto plain=renderGestureWithPreset(samples,Tool::Paint,c,empty,target);
    const auto mirrored=renderGestureWithPreset(samples,Tool::Paint,c,symmetry,target);
    if(!plain || !mirrored || alpha(*plain,true)!=0 || alpha(*mirrored,true)==0) {
        std::cerr << "symmetry did not alter actual pixels\n"; return 4;
    }
    BrushPreset pressure;
    pressure.dynamics.opacityResponse.enabled=true;
    pressure.dynamics.opacityResponse.pressure={true,.25,.25,.25};
    const auto light=renderGestureWithPreset(samples,Tool::Paint,c,pressure,target);
    if(!light || alpha(*light)==0 || alpha(*light)>=alpha(*plain)) {
        std::cerr << "pressure curve did not alter actual alpha\n"; return 5;
    }
    BrushPreset warmup; warmup.stroke.warmupDistance=30;
    const auto warmed=renderGestureWithPreset(samples,Tool::Paint,c,warmup,target);
    if(!warmed || alpha(*warmed)==0 || alpha(*warmed)>=alpha(*plain)) {
        std::cerr << "independent preset alpha warmup did not affect pixels\n"; return 9;
    }
    auto bad=symmetry; bad.stroke.symmetry.center.x=std::numeric_limits<double>::quiet_NaN();
    if(resolveWithPreset(Tool::Paint,c,bad,target.argb) ||
       renderGestureWithPreset(samples,Tool::Paint,c,bad,target)) {
        std::cerr << "invalid preset accepted\n"; return 6;
    }
    auto unsupported=empty; unsupported.stroke.blendMode=RasterBlendMode::Multiply;
    if(resolveWithPreset(Tool::Paint,c,unsupported,target.argb) ||
       renderGestureWithPreset(samples,Tool::Paint,c,unsupported,target)) {
        std::cerr << "unsupported preset blend was silently ignored\n"; return 7;
    }
    BrushPreset customTip;
    customTip.tip={2,2,std::vector<std::byte>(4,std::byte{255})};
    BrushPreset customSequence;
    customSequence.tipSequence.tips.push_back(customTip.tip);
    const auto customPaint=renderGestureWithPreset(samples,Tool::Paint,c,customTip,target);
    if(!customPaint || equal(*customPaint,*plain)) {
        std::cerr << "supported Paint custom tip did not affect pixels\n"; return 10;
    }
    for(const auto *custom : {&customTip,&customSequence}) {
        if(resolveWithPreset(Tool::Bristle,c,*custom,target.argb) ||
           renderGestureWithPreset(samples,Tool::Bristle,c,*custom,target) ||
           availabilityWithPreset(Tool::Bristle,Parameter::BristleCount,c,*custom).reason!=InactiveReason::Invalid) {
            std::cerr << "unsupported custom Bristle tip was silently overridden or shadowed\n"; return 8;
        }
    }
    std::cout << "legacy_parity=paint,eraser symmetry_pixels=pass pressure_alpha=pass invalid=reject\n";
}
