#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "Brush/BrushResolve.h"
#include "Brush/BrushPresetSerializer.h"
#include "Document/DocumentSerializer.h"
#include "Input/InputNormalizer.h"
#include "Input/InputStrokeBuilder.h"
#include "Layer/RasterLayer.h"
#include "RasterDabTestUtils.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace {
bool near(double a, double b) { return std::abs(a-b)<1e-8; }
std::uint8_t alphaAt(const std::vector<RasterSample> &pixels, int x, int y)
{
    for (const auto &p : pixels) if (p.position.x == x && p.position.y == y) return p.argb >> 24;
    return 0;
}
BrushDynamicsBinding binding(BrushDynamicsSource source, BrushDynamicsTarget target, double minimum=0, double maximum=1)
{
    BrushDynamicsBinding b;
    b.source=source; b.target=target; b.inputMinimum=minimum; b.inputMaximum=maximum;
    b.combineMode=BrushDynamicsCombineMode::Replace;
    return b;
}
std::vector<RasterSample> stamp(const BrushPreset &p, double pressure=1)
{
    const auto r=resolveBrushPreset(p);
    if (!r.brush) return {};
    RasterDabStream stream;
    const auto dabs=appendRasterDabs(stream, StrokePoint{{40,40},pressure}, *r.brush);
    return projectBrushDabs(dabs,r.brush->rasterizer,{},r.brush->material);
}
}
int main()
{
    BrushPreset preset;
    preset.size=20; preset.shape.angleMode=BrushAngleMode::Fixed;
    const auto hard=stamp(preset);
    preset.hardness=0;
    const auto soft=stamp(preset);
    CHECK(alphaAt(hard,40,40)==255 && alphaAt(soft,40,40)==255);
    CHECK(alphaAt(hard,46,40)>alphaAt(soft,46,40));
    preset.hardness=1; preset.shape.roundness=0.25;
    const auto flat=stamp(preset);
    CHECK(alphaAt(flat,47,40)>0 && alphaAt(flat,40,47)==0);
    preset.shape.angleRadians=1.5707963267948966;
    const auto turned=stamp(preset);
    CHECK(alphaAt(turned,47,40)==0 && alphaAt(turned,40,47)>0);
    preset.shape={}; preset.shape.angleMode=BrushAngleMode::Fixed;
    preset.shape.kind=BrushTipShape::Square;
    CHECK(alphaAt(stamp(preset),47,47)>0);
    preset.shape.kind=BrushTipShape::Diamond;
    CHECK(alphaAt(stamp(preset),47,47)==0);

    // Different dimensions and masks are selected and projected without copying the bank per stamp.
    preset.shape={}; preset.shape.angleMode=BrushAngleMode::Fixed; preset.size=12;
    preset.tipSequence.tips={{3,1,{std::byte{255},std::byte{0},std::byte{0}}},
                             {1,3,{std::byte{0},std::byte{255},std::byte{0}}}};
    auto brush=resolveBrushPreset(preset,0xFFCC4400U,13);
    CHECK(brush.brush);
    RasterDabStream stream;
    const auto line=streamTestDabs(*brush.brush,StrokePoint{{40,40},1,0},StrokePoint{{52,40},1,1});
    CHECK(line.size()>2 && line[0].tipIndex==0 && line[1].tipIndex==1 && line[2].tipIndex==0);
    const auto asym=stamp(preset);
    CHECK(alphaAt(asym,36,40)>alphaAt(asym,44,40));
    preset.shape.flipX=true;
    const auto flipped=stamp(preset);
    CHECK(alphaAt(flipped,36,40)<alphaAt(flipped,44,40));
    preset.tipSequence.selection=BrushTipSelection::Pressure;
    preset.shape.flipX=false;
    CHECK(alphaAt(stamp(preset,0),36,40)>0);
    CHECK(alphaAt(stamp(preset,1),36,40)==0);
    preset.tipSequence.selection=BrushTipSelection::Random;
    brush=resolveBrushPreset(preset,0xFFCC4400U,13);
    const auto random=streamTestDabs(*brush.brush,StrokePoint{{40,40},1,0},StrokePoint{{100,40},1,1});
    const auto again=streamTestDabs(*brush.brush,StrokePoint{{40,40},1,0},StrokePoint{{100,40},1,1});
    bool varied=false;
    for (std::size_t i=0;i<random.size();++i) { CHECK(random[i].tipIndex==again[i].tipIndex); varied |= random[i].tipIndex!=random[0].tipIndex; }
    CHECK(varied);
    const auto pixels=projectBrushDabs(random,brush.brush->rasterizer);
    const auto bounds=deviceBoundsForBrushDabsUnion(random,brush.brush->rasterizer,{});
    for (const auto &px:pixels) CHECK(px.position.x>=bounds.origin.x && px.position.x<bounds.origin.x+bounds.width
                                   && px.position.y>=bounds.origin.y && px.position.y<bounds.origin.y+bounds.height);

    // Every supported sensor channel is routed through its own normalized range.
    BrushDynamicsInput input;
    input.pressure=.4; input.velocity=40; input.tiltX=.24; input.tiltY=.32;
    input.directionRadians=.4; input.rotationRadians=.4; input.tangentialPressure=.4;
    input.distance=40; input.elapsedTime=4; input.randomGrain=.4; input.strokeRandom=.4; input.custom[3]=4;
    for (const auto source:{BrushDynamicsSource::Pressure,BrushDynamicsSource::Velocity,BrushDynamicsSource::Tilt,
                           BrushDynamicsSource::Direction,BrushDynamicsSource::Rotation,BrushDynamicsSource::TangentialPressure,
                           BrushDynamicsSource::Distance,BrushDynamicsSource::Time,BrushDynamicsSource::Random,
                           BrushDynamicsSource::StrokeRandom,BrushDynamicsSource::Custom}) {
        BrushDynamics dynamics;
        auto b=binding(source,BrushDynamicsTarget::Flow);
        if (source==BrushDynamicsSource::Velocity || source==BrushDynamicsSource::Distance) b.inputMaximum=100;
        if (source==BrushDynamicsSource::Time || source==BrushDynamicsSource::Custom) b.inputMaximum=10;
        b.customInput=3; dynamics.bindings.push_back(b);
        CHECK(near(resolveBrushDynamics(dynamics,input).flowScale,.4));
    }
    BrushDynamics dynamics;
    dynamics.bindings={binding(BrushDynamicsSource::TiltX,BrushDynamicsTarget::Hardness),
                       binding(BrushDynamicsSource::TiltY,BrushDynamicsTarget::Roundness)};
    auto result=resolveBrushDynamics(dynamics,input);
    CHECK(near(result.hardnessScale,.24) && near(result.roundnessScale,.32));
    dynamics.tiltInputEnabled=false; result=resolveBrushDynamics(dynamics,input);
    CHECK(result.hardnessScale==1 && result.roundnessScale==1);
    dynamics={};
    auto b=binding(BrushDynamicsSource::Pressure,BrushDynamicsTarget::Size);
    b.curve.points={{0,0},{.2,.5},{.6,.7},{1,1}};
    dynamics.bindings={b}; CHECK(near(resolveBrushDynamics(dynamics,input).sizeScale,.6));
    b.combineMode=BrushDynamicsCombineMode::Multiply; b.curve.points={{0,.5},{1,.5}};
    dynamics.bindings.push_back(b); CHECK(near(resolveBrushDynamics(dynamics,input).sizeScale,.3));
    b.combineMode=BrushDynamicsCombineMode::Add; dynamics.bindings.push_back(b);
    CHECK(near(resolveBrushDynamics(dynamics,input).sizeScale,.8));

    // Color bindings reach actual pixels. Full hue turn fraction 1/3 changes red to green.
    preset={}; preset.color.enabled=true;
    b=binding(BrushDynamicsSource::Custom,BrushDynamicsTarget::Hue);
    b.curve.points={{0,1.0/3},{1,1.0/3}}; preset.dynamics.bindings={b};
    brush=resolveBrushPreset(preset,0xFFFF0000U,99);
    auto dabs=appendRasterDabs(stream,StrokePoint{{40,40}},*brush.brush,true);
    CHECK(dabs.front().colorArgb==0xFF00FF00U);
    auto green=projectBrushDabs(dabs,brush.brush->rasterizer);
    CHECK(!green.empty() && (green.front().argb&0xFFFFFFU)==0x00FF00U);
    preset.dynamics.bindings.clear(); preset.color.hueJitter=.4;
    brush=resolveBrushPreset(preset,0xFFFF0000U,99);
    const auto colors=streamTestDabs(*brush.brush,StrokePoint{{0,0},1,0},StrokePoint{{30,0},1,1});
    CHECK(colors[0].colorArgb!=colors[1].colorArgb);
    preset.color.perStroke=true;
    brush=resolveBrushPreset(preset,0xFFFF0000U,99);
    const auto oneColor=streamTestDabs(*brush.brush,StrokePoint{{0,0},1,0},StrokePoint{{30,0},1,1});
    for (const auto &dab:oneColor) CHECK(dab.colorArgb==oneColor[0].colorArgb);
    preset.dynamics.randomInputEnabled=false;
    brush=resolveBrushPreset(preset,0xFFFF0000U,99);
    resetRasterDabStream(stream);
    CHECK(appendRasterDabs(stream,StrokePoint{},*brush.brush)[0].colorArgb==0xFFFF0000U);

    // Distance/time dynamics and spacing consume the complete application input.
    preset={}; preset.size=10; preset.stroke.spacing=2;
    b=binding(BrushDynamicsSource::Custom,BrushDynamicsTarget::Spacing); b.customInput=2;
    preset.dynamics.bindings={b}; brush=resolveBrushPreset(preset);
    StrokePoint from{{0,0},1,0}, to{{10,0},1,1}; from.custom[2]=to.custom[2]=.5;
    const auto spaced=streamTestDabs(*brush.brush,from,to);
    CHECK(spaced.size()==11 && near(spaced[1].position.x,1));
    preset.stroke.airbrushEnabled=true; preset.stroke.airbrushRate=4;
    b=binding(BrushDynamicsSource::Time,BrushDynamicsTarget::Flow); preset.dynamics.bindings={b};
    brush=resolveBrushPreset(preset); resetRasterDabStream(stream);
    CHECK(appendRasterDabs(stream,from,*brush.brush).front().alpha==0);
    auto spray=appendRasterDabs(stream,to,*brush.brush,true);
    CHECK(spray.size()==4 && near(spray.front().alpha,.25) && near(spray.back().alpha,1));
    CHECK(near(spray.front().position.x,2.5));

    // Scatter follows movement rather than tip angle; reproducibility survives event subdivision.
    preset={}; preset.size=4; preset.stroke.spacing=2;
    preset.material.scatter.enabled=true; preset.material.scatter.count=5; preset.material.scatter.radius=2;
    preset.material.scatter.axes=BrushScatterAxes::Perpendicular;
    preset.material.scatter.distribution=BrushScatterDistribution::Disk;
    preset.material.scatter.countJitter=.5;
    brush=resolveBrushPreset(preset,0xFF225577U,123);
    const auto whole=streamTestDabs(*brush.brush,StrokePoint{{0,0},1,0},StrokePoint{{20,0},1,2});
    const auto split=streamTestDabs(*brush.brush,StrokePoint{{0,0},1,0},StrokePoint{{10,0},1,1},StrokePoint{{20,0},1,2});
    CHECK(whole.size()==split.size() && whole.size()<55);
    for (std::size_t i=0;i<whole.size();++i) {
        CHECK(whole[i].position.x==split[i].position.x && whole[i].position.y==split[i].position.y);
        CHECK(std::abs(whole[i].position.y)<=2);
    }

    preset.size=4096;
    preset.material.scatter.radius=1e6;
    preset.material.scatter.relativeToSize=true;
    b=binding(BrushDynamicsSource::Pressure,BrushDynamicsTarget::Scatter);
    b.curve.points={{0,1024},{1,1024}};
    preset.dynamics.bindings={b};
    brush=resolveBrushPreset(preset); CHECK(brush.brush);
    resetRasterDabStream(stream);
    for (const auto &dab:appendRasterDabs(stream,StrokePoint{},*brush.brush,true))
        CHECK(std::isfinite(dab.position.y) && std::abs(dab.position.y)<=1e6);
    preset.size=4;
    preset.material.scatter.radius=2;

    // Native input forwards barrel pressure and custom application channels.
    TabletState tablet; tablet.contact=true; tablet.pressure=.8; tablet.tangentialPressure=-.3; tablet.custom[4]=.7;
    auto event=normalizeTabletPointerEvent({},tablet,PointerEventPhase::Press);
    InputStrokeBuilder builder;
    const auto point=appendPointerEvent(builder,event).point;
    CHECK(near(point.tangentialPressure,-.3) && near(point.custom[4],.7));

    // New presets and document sources preserve every extension, including string escapes.
    preset.name="한국어\tbrush\n\\quoted\"";
    preset.tipSequence.tips={{1,1,{std::byte{127}}}};
    preset.dynamics.bindings={b}; preset.shape.flipY=true; preset.color.enabled=true;
    const auto payload=serializeBrushPreset(preset);
    CHECK(serializeBrushPreset(deserializeBrushPreset(payload))==payload);
    DocumentArchive archive;
    archive.brushSources.push_back(snapshotBrushPreset(preset));
    const auto archivePayload=serializeDocumentArchive(archive);
    const auto document=deserializeDocumentArchive(archivePayload);
    CHECK(document.compatible && document.brushSources.size()==1);
    CHECK(serializeBrushPreset(restoreBrushPreset(document.brushSources[0]))==payload);
    auto damagedDocument=archivePayload;
    const auto embedded=damagedDocument.find(".presetV2\t")+10;
    damagedDocument[embedded]='Z';
    CHECK(!deserializeDocumentArchive(damagedDocument).compatible);
    const std::string legacy="formatMagic\t\"iiPaintBrushPreset\"\nformatVersion\t1\nsize\t12\nflow\t1\nopacity\t1\nhardness\t1\ndensity\t1\n";
    const auto old=readBrushPreset(legacy); CHECK(old.preset && resolveBrushPreset(*old.preset).brush);
    for (const auto &bad:{std::string(""),legacy+"size\t3\n",legacy+"tip.width\t4\n",
                         legacy+"dynamics.bindings.count\t999999999\n",legacy+"tip.mask\tABZ\n",
                         legacy+"shape.kind\t99\n",legacy+"stroke.spacing\tnan\n",
                         legacy+"color.enabled\t2\n",legacy+"tipSequence.count\t-1\n",
                         legacy+"dynamics.bindings.count\t1\ndynamics.bindings.0.curve.points.count\t99999\n"})
        CHECK(!readBrushPreset(bad).preset);
    preset.dynamics.bindings[0].curve.points={{.5,0},{.5,1}};
    CHECK(!resolveBrushPreset(preset).brush);
    preset={}; preset.tip={1,1,{std::byte{255},std::byte{255}}}; CHECK(!resolveBrushPreset(preset).brush);

    // Bad events cannot partially advance a stroke or consume its random sequence.
    preset={}; brush=resolveBrushPreset(preset); resetRasterDabStream(stream);
    appendRasterDabs(stream,StrokePoint{{0,0},1,2},*brush.brush);
    const auto saved=stream;
    bool rejected=false;
    try { appendRasterDabs(stream,StrokePoint{{1,0},1,1},*brush.brush); } catch (const std::invalid_argument &) { rejected=true; }
    CHECK(rejected && stream.sequenceIndex==saved.sequenceIndex && stream.previousPoint.time==2);
    auto invalid=StrokePoint{{1,0},1,3}; invalid.pressure=std::numeric_limits<double>::infinity(); rejected=false;
    try { appendRasterDabs(stream,invalid,*brush.brush); } catch (const std::invalid_argument &) { rejected=true; }
    CHECK(rejected && stream.sequenceIndex==saved.sequenceIndex);
    return 0;
}
