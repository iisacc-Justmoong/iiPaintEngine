#include "Controls.h"
#include <Brush/BrushResolve.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace ii::brush {
namespace {
using P = Parameter;
constexpr ParameterInfo infos[]{
    {P::Size,"size",Unit::Pixels,1,1024,64},
    {P::Hardness,"hardness",Unit::Percent,0,100,85},
    {P::Opacity,"opacity",Unit::Percent,0,100,100},
    {P::Flow,"flow",Unit::Percent,0,100,90},
    {P::Roundness,"roundness",Unit::Percent,0,100,100},
    {P::Angle,"angle",Unit::Degrees,-180,180,0},
    {P::Spacing,"spacing",Unit::Percent,0,100,12},
    {P::Scatter,"scatter",Unit::Percent,0,100,5},
    {P::BristleCount,"bristle-count",Unit::Count,1,64,1},
    {P::Stabilization,"stabilization",Unit::Response,0,100,18},
    {P::PostCorrection,"post-correction",Unit::Pixels,0,20,4},
    {P::Pressure,"pressure",Unit::Percent,0,100,85},
    {P::Tilt,"tilt",Unit::Percent,0,100,30},
    {P::Velocity,"velocity",Unit::Percent,0,100,10},
    {P::Wetness,"wetness",Unit::Percent,0,100,20},
    {P::Mix,"mix",Unit::Percent,0,100,45},
    {P::Load,"load",Unit::Percent,0,100,80},
    {P::Dilution,"dilution",Unit::Percent,0,100,12},
    {P::Start,"start",Unit::Percent,0,100,15},
    {P::End,"end",Unit::Percent,0,100,30},
    {P::MinimumSize,"minimum-size",Unit::Percent,0,100,8},
    {P::TaperStrength,"taper-strength",Unit::Percent,0,100,35}
};
static_assert(std::size(infos)==static_cast<std::size_t>(P::Count));
bool validTool(Tool tool) noexcept
{ return tool>=Tool::Paint && tool<=Tool::Eraser; }
bool customBristleTip(Tool tool, const BrushPreset& base) noexcept
{ return tool==Tool::Bristle && (!base.tip.mask.empty() || !base.tipSequence.tips.empty()); }
// A generated bitmap tip, rather than the engine's saturated ellipse approximation.
void bristleTip(BrushPreset& preset, unsigned count, double hardness)
{
    constexpr int side=128;
    preset.tip.width=side; preset.tip.height=side;
    preset.tip.mask.assign(side*side,std::byte{});
    for(unsigned i=0;i<count;++i) {
        const double radius=i==0?0:.4*std::sqrt(i/63.);
        const double angle=i*2.399963229728653;
        const double cx=.5+radius*std::cos(angle), cy=.5+radius*std::sin(angle);
        for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
            const double distance=std::hypot((x+.5)/side-cx,(y+.5)/side-cy)/.055;
            const double alpha=std::clamp((1-distance)/std::max(.001,1-hardness),0.,1.);
            auto& value=preset.tip.mask[static_cast<std::size_t>(y)*side+x];
            value=static_cast<std::byte>(std::max(std::to_integer<int>(value),static_cast<int>(std::lround(alpha*255))));
        }
    }
}
}
std::span<const ParameterInfo> parameterInfo() noexcept { return infos; }
Controls::Controls()
{ for(const auto& info:infos) m_values[static_cast<std::size_t>(info.parameter)]=info.initial; }
double Controls::get(Parameter parameter) const { return m_values.at(static_cast<std::size_t>(parameter)); }
bool Controls::set(Parameter parameter, double value)
{
    const auto index=static_cast<std::size_t>(parameter);
    if(index>=std::size(infos) || !std::isfinite(value)) return false;
    const auto& info=infos[index];
    if(value<info.minimum || value>info.maximum || (info.unit==Unit::Count && std::trunc(value)!=value)) return false;
    m_values[index]=value; return true;
}
std::string Controls::serialize() const
{
    std::ostringstream result;
    result.imbue(std::locale::classic());
    result << "iibrushtools.controls=1\n" << std::setprecision(std::numeric_limits<double>::max_digits10);
    for(const auto& info:infos) {
        result << info.key << '=' << get(info.parameter) << '\n';
    }
    if(!result) throw std::logic_error("control serialization failed");
    return result.str();
}
bool Controls::restore(std::string_view text)
{
    constexpr std::string_view header="iibrushtools.controls=1\n";
    if(text.size()>8192 || !text.starts_with(header)) return false;
    text.remove_prefix(header.size()); Controls next;
    std::array<bool,static_cast<std::size_t>(P::Count)> seen{};
    while(!text.empty()) {
        const auto end=text.find('\n'); const auto line=text.substr(0,end);
        text=end==text.npos?std::string_view{}:text.substr(end+1);
        const auto eq=line.find('=');if(eq==line.npos) return false;
        const auto key=line.substr(0,eq),number=line.substr(eq+1);
        const auto info=std::ranges::find(infos,key,&ParameterInfo::key);
        if(info==std::end(infos) || number.empty()) return false;
        const auto index=static_cast<std::size_t>(info->parameter);
        if(seen[index]) return false;
        double value=0;
        // Classic-locale streams support older macOS deployment targets where
        // floating-point charconv symbols are unavailable. Keep from_chars'
        // no-leading-plus and no-whitespace rules for the existing format.
        if(number.front()=='+' || number.front()==' ' || number.front()=='\t' || number.front()=='\r') return false;
        std::istringstream input{std::string(number)};
        input.imbue(std::locale::classic());
        input >> std::noskipws >> value;
        if(!input || input.peek()!=std::char_traits<char>::eof() || !next.set(info->parameter,value)) return false;
        seen[index]=true;
    }
    if(!std::ranges::all_of(seen,[](bool value){return value;})) return false;
    m_values=next.m_values;return true;
}
Availability availability(Tool tool, Parameter parameter, const Controls& controls) noexcept
{
    if(!validTool(tool) || parameter>=P::Count) return {false,InactiveReason::Invalid};
    if(parameter==P::Wetness || parameter==P::Mix || parameter==P::Load || parameter==P::Dilution)
        return {tool==Tool::Mixer || tool==Tool::Smudge,
            tool==Tool::Mixer || tool==Tool::Smudge?InactiveReason::None:InactiveReason::ToolMismatch};
    if(parameter==P::BristleCount) return {tool==Tool::Bristle,
        tool==Tool::Bristle?InactiveReason::None:InactiveReason::ToolMismatch};
    if(parameter==P::Spacing && tool==Tool::Airbrush) return {false,InactiveReason::TimePlacement};
    if((parameter==P::MinimumSize || parameter==P::TaperStrength)
        && controls.get(P::Start)==0 && controls.get(P::End)==0) return {false,InactiveReason::TaperDisabled};
    return {true,InactiveReason::None};
}
Availability availabilityWithPreset(Tool tool, Parameter parameter, const Controls& controls,
                                    const BrushPreset& base) noexcept
{
    if(base.stroke.blendMode!=RasterBlendMode::SourceOver || customBristleTip(tool,base))
        return {false,InactiveReason::Invalid};
    const auto basic=availability(tool,parameter,controls);
    if(!basic.enabled) return basic;
    const auto disabled=Availability{false,InactiveReason::PresetDisabled};
    if(parameter==P::Spacing) {
        if(!std::isfinite(base.stroke.spacing) || base.stroke.spacing<0 || base.stroke.spacing>1e6)
            return {false,InactiveReason::Invalid};
        if(!base.stroke.spacingEnabled) return disabled;
        if(base.stroke.spacing>0) return {false,InactiveReason::PresetOverride};
    }
    if(parameter==P::Opacity && !base.stroke.opacityEnabled) return disabled;
    if(parameter==P::Flow && !base.stroke.flowEnabled) return disabled;
    // Bristle tip generation also uses hardness even if raster hardness is off.
    if(parameter==P::Hardness && !base.stroke.hardnessEnabled && tool!=Tool::Bristle) return disabled;
    if(parameter==P::Pressure && (!base.dynamics.pressureInputEnabled ||
       (!base.dynamics.pressureToSizeEnabled && !base.dynamics.pressureToFlowEnabled))) return disabled;
    if(parameter==P::Tilt && (!base.dynamics.tiltInputEnabled || !base.dynamics.tiltToEllipseEnabled)) return disabled;
    if(parameter==P::Velocity && !base.dynamics.velocityInputEnabled) return disabled;
    return basic;
}
std::expected<ResolvedBrush, Error> resolve(Tool tool, const Controls& controls, std::uint32_t argb,
    std::uint32_t seed, const DynamicsContext& context)
{
    return resolveWithPreset(tool,controls,BrushPreset{},argb,seed,context);
}
std::expected<ResolvedBrush, Error> resolveWithPreset(Tool tool, const Controls& controls,
    const BrushPreset& base, std::uint32_t argb, std::uint32_t seed, const DynamicsContext& context)
{
    if(!validTool(tool)) return std::unexpected(Error{ErrorCode::InvalidControls,"unknown brush tool"});
    if(!std::isfinite(context.velocityReferenceSpeed) || context.velocityReferenceSpeed<=0)
        return std::unexpected(Error{ErrorCode::InvalidControls,"velocity reference speed must be finite and positive"});
    // The tiled authoring compositor currently uses SourceOver (or erasure).
    // Do not silently accept a preset blend mode that it cannot honor.
    if(base.stroke.blendMode!=RasterBlendMode::SourceOver)
        return std::unexpected(Error{ErrorCode::InvalidControls,"preset blend mode is unsupported by authoring"});
    // A Bristle stroke generates its own count-dependent tip. A supplied tip
    // would otherwise be discarded, or a tip sequence would shadow it.
    if(customBristleTip(tool,base))
        return std::unexpected(Error{ErrorCode::InvalidControls,"custom Bristle tips are unsupported by authoring"});
    const auto ratio=[&](P p){return controls.get(p)/100.;};
    BrushPreset preset=base;
    // Position correction is applied by this authoring layer. The preset's
    // alpha warmup is a separate effect from whole-stroke size taper.
    preset.stroke.smoothingEnabled=false;
    preset.material.simulation.enabled=tool==Tool::Mixer || tool==Tool::Smudge;
    preset.material.bristle.enabled=tool==Tool::Bristle;
    preset.size=static_cast<float>(controls.get(P::Size));
    preset.opacity=static_cast<float>(ratio(P::Opacity));preset.flow=static_cast<float>(ratio(P::Flow));
    preset.hardness=static_cast<float>(ratio(P::Hardness));
    preset.shape.angleMode=BrushAngleMode::Fixed;
    preset.shape.angleRadians=controls.get(P::Angle)*std::numbers::pi/180;
    preset.shape.roundness=std::max(.01,ratio(P::Roundness));
    preset.stroke.spacingRatio=ratio(P::Spacing);
    preset.stroke.airbrushEnabled=tool==Tool::Airbrush;
    preset.stroke.airbrushRate=30;
    preset.dynamics.pressureToSize=ratio(P::Pressure);
    preset.dynamics.pressureToFlow=ratio(P::Pressure);
    preset.dynamics.tiltToEllipse=ratio(P::Tilt);
    if(controls.get(P::Velocity)>0) {
        BrushDynamicsBinding velocity;
        velocity.source=BrushDynamicsSource::Velocity;
        velocity.target=BrushDynamicsTarget::Opacity;
        velocity.inputMaximum=context.velocityReferenceSpeed;
        const auto amount=ratio(P::Velocity);
        velocity.curve={true,1,1-amount/2,1-amount};
        preset.dynamics.bindings.push_back(std::move(velocity));
    }
    preset.material.scatter.enabled=controls.get(P::Scatter)>0;
    preset.material.scatter.radius=ratio(P::Scatter);
    preset.material.scatter.relativeToSize=true;
    if(tool==Tool::Bristle) {
        preset.material.bristle.enabled=true;
        preset.material.bristle.count=static_cast<unsigned>(controls.get(P::BristleCount));
        bristleTip(preset,preset.material.bristle.count,ratio(P::Hardness));
    }
    if(tool==Tool::Mixer || tool==Tool::Smudge) {
        auto& simulation=preset.material.simulation;
        simulation.enabled=true;
        simulation.model=tool==Tool::Mixer?BrushSimulationModel::Mixer:BrushSimulationModel::Smudge;
        simulation.wetness=ratio(P::Wetness);simulation.mixStrength=ratio(P::Mix);
        simulation.smudgeStrength=tool==Tool::Smudge?ratio(P::Mix):0;
        simulation.load=ratio(P::Load);
        simulation.pickup=ratio(P::Dilution);simulation.deposit=1-ratio(P::Dilution);
    }
    const auto tipColor=tool==Tool::Eraser?argb|0xff000000U:argb;
    auto runtime=resolveBrushPreset(preset,tipColor,seed);
    if(!runtime.brush) return std::unexpected(Error{ErrorCode::InvalidControls,
        runtime.errors.empty()?"brush resolve failed":runtime.errors.front()});
    ResolvedBrush result;
    result.state=std::move(*runtime.brush);
    result.taper={ratio(P::Start),ratio(P::End),ratio(P::MinimumSize),ratio(P::TaperStrength)};
    result.stabilization=ratio(P::Stabilization);result.postCorrection=controls.get(P::PostCorrection);
    result.eraser=tool==Tool::Eraser;result.hairline=controls.get(P::Roundness)==0;
    return result;
}
double taperSize(const TaperProfile& profile, double distance, double totalLength) noexcept
{
    if(!std::isfinite(totalLength) || totalLength<=0 || !std::isfinite(distance)
        || !std::isfinite(profile.start) || !std::isfinite(profile.end)
        || !std::isfinite(profile.minimumSize) || !std::isfinite(profile.strength)) return 1;
    const auto progress=[](double value){const auto t=std::clamp(value,0.,1.);return t*t*(3-2*t);};
    double factor=1;
    if(profile.start>0) factor=std::min(factor,progress(distance/(totalLength*profile.start)));
    if(profile.end>0) factor=std::min(factor,progress((totalLength-distance)/(totalLength*profile.end)));
    return std::clamp(1-std::clamp(profile.strength,0.,1.)*(1-std::clamp(profile.minimumSize,0.,1.))*(1-factor),0.,1.);
}
}
