// Preset policy must explain when its documented flags override a control.
#include <iiBrushTools/Gesture.h>
#include <Brush/BrushPreset.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
using namespace ii::brush;
namespace {
bool equivalent(const RenderedStroke &a,const RenderedStroke &b) {
    if(a.tiles.size()!=b.tiles.size() || a.dabCount!=b.dabCount) return false;
    for(std::size_t i=0;i<a.tiles.size();++i) {
        const auto &x=a.tiles[i],&y=b.tiles[i];
        if(x.bounds.origin.x!=y.bounds.origin.x || x.bounds.origin.y!=y.bounds.origin.y ||
           x.bounds.width!=y.bounds.width || x.bounds.height!=y.bounds.height ||
           x.pixels.pixels!=y.pixels.pixels) return false;
    }
    return true;
}
Controls plain() {
    Controls c;
    for(const auto &[p,v] : std::array<std::pair<Parameter,double>,9>{{
        {Parameter::Size,10},{Parameter::Hardness,100},{Parameter::Flow,40},
        {Parameter::Pressure,0},{Parameter::Velocity,0},{Parameter::Tilt,0},
        {Parameter::Scatter,0},{Parameter::Stabilization,0},{Parameter::PostCorrection,0}}})
        if(!c.set(p,v)) throw std::runtime_error("bad fixture controls");
    c.set(Parameter::Start,0); c.set(Parameter::End,0); return c;
}
bool unchanged(BrushPreset preset,Parameter p,double a,double b) {
    auto c=plain(), d=c; if(!c.set(p,a)||!d.set(p,b)) return false;
    std::array<StrokePoint,2> samples{};
    samples[0].position={16,32}; samples[1].position={144,32};
    samples[0].pressure=samples[1].pressure=.4;
    samples[0].tiltX=samples[1].tiltX=.7;
    samples[0].time=1; samples[1].time=1.1;
    RenderTarget target; target.clip={{0,0},192,64};
    const auto x=renderGestureWithPreset(samples,Tool::Paint,c,preset,target);
    const auto y=renderGestureWithPreset(samples,Tool::Paint,d,preset,target);
    return x && y && equivalent(*x,*y);
}
}
int main() {
    const auto c=plain(); BrushPreset preset;
    if(!availability(Tool::Paint,Parameter::Spacing,c).enabled) return 1;
    preset.stroke.spacing=3;
    auto status=availabilityWithPreset(Tool::Paint,Parameter::Spacing,c,preset);
    if(status.enabled || status.reason!=InactiveReason::PresetOverride ||
       !unchanged(preset,Parameter::Spacing,5,90) || unchanged(BrushPreset{},Parameter::Spacing,5,90)) {
        std::cerr << "absolute spacing did not disable the ineffective ratio\n"; return 2;
    }
    status=availabilityWithPreset(Tool::Airbrush,Parameter::Spacing,c,preset);
    if(status.enabled || status.reason!=InactiveReason::TimePlacement) return 3;
    preset={}; preset.stroke.spacingEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Spacing,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Spacing,5,90)) return 4;
    preset={}; preset.stroke.opacityEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Opacity,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Opacity,20,80) || unchanged(BrushPreset{},Parameter::Opacity,20,80)) return 5;
    preset={}; preset.stroke.flowEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Flow,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Flow,20,80) || unchanged(BrushPreset{},Parameter::Flow,20,80)) return 6;
    preset={}; preset.stroke.hardnessEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Hardness,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Hardness,20,80) ||
       !availabilityWithPreset(Tool::Bristle,Parameter::Hardness,c,preset).enabled) return 7;
    preset={}; preset.dynamics.pressureInputEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Pressure,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Pressure,0,100)) return 8;
    preset={}; preset.dynamics.tiltInputEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Tilt,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Tilt,0,100)) return 9;
    preset={}; preset.dynamics.velocityInputEnabled=false;
    if(availabilityWithPreset(Tool::Paint,Parameter::Velocity,c,preset).reason!=InactiveReason::PresetDisabled ||
       !unchanged(preset,Parameter::Velocity,0,100)) return 10;
    preset={}; preset.stroke.spacing=std::numeric_limits<double>::quiet_NaN();
    if(availabilityWithPreset(Tool::Paint,Parameter::Spacing,c,preset).reason!=InactiveReason::Invalid) return 11;
    std::cout << "preset_availability=pixel_checked disabled_spacing_opacity_flow_pressure_tilt_velocity=pass\n";
}
