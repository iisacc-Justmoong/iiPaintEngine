#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include "iiPaintEngine"
#include "TestDirectory.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace {
bool near(double a, double b) { return std::abs(a-b) < 1e-7; }
bool at(const BrushDab &d, double x, double y) { return near(d.position.x,x) && near(d.position.y,y); }
PointerEvent event(PointerEventPhase phase, DocumentPoint position, double time) {
    PointerEvent p;
    p.phase=phase; p.documentPosition=position; p.time=time;
    p.button=PointerButton::Primary; p.primaryButtonDown=phase!=PointerEventPhase::Release;
    return p;
}
}
int main()
{
    BrushPreset preset; preset.size=3; preset.stroke.spacing=2;
    preset.stroke.symmetry.mode=BrushSymmetryMode::Vertical;
    preset.stroke.symmetry.center={24,24};
    auto brush=resolveBrushPreset(preset,0xFFFF0000U,41);
    CHECK(brush.brush);
    RasterDabStream stream;
    auto dabs=appendRasterDabs(stream,StrokePoint{{30,27},.6,0},*brush.brush);
    CHECK(dabs.size()==2 && at(dabs[0],30,27) && at(dabs[1],18,27));
    CHECK(dabs[0].sequenceIndex==dabs[1].sequenceIndex && stream.sequenceIndex==1);
    CHECK(dabs[0].randomSeed==dabs[1].randomSeed && dabs[0].colorArgb==dabs[1].colorArgb);
    CHECK(dabs[0].flipX!=dabs[1].flipX);
    CHECK(near(std::sin(dabs[1].rotationRadians),-std::sin(dabs[0].rotationRadians)));

    preset.stroke.symmetry.mode=BrushSymmetryMode::Radial;
    preset.stroke.symmetry.segments=16;
    brush=resolveBrushPreset(preset); resetRasterDabStream(stream);
    dabs=appendRasterDabs(stream,StrokePoint{{30,24}},*brush.brush);
    CHECK(dabs.size()==16 && std::ranges::any_of(dabs,[](const auto &d){return at(d,24,30);}));
    CHECK(stream.sequenceIndex==1);
    preset.stroke.symmetry.mode=BrushSymmetryMode::Mandala;
    preset.stroke.symmetry.segments=3;
    brush=resolveBrushPreset(preset); resetRasterDabStream(stream);
    dabs=appendRasterDabs(stream,StrokePoint{{30,26}},*brush.brush);
    CHECK(dabs.size()==6);
    resetRasterDabStream(stream);
    CHECK(appendRasterDabs(stream,StrokePoint{{24,24}},*brush.brush).size()==1);

    // Reflection must mirror an asymmetric bitmap tip, not only its stamp center.
    preset.stroke.symmetry.mode=BrushSymmetryMode::Vertical;
    preset.tip={2,1,{std::byte{255},std::byte{0}}}; preset.size=4;
    preset.shape.angleMode=BrushAngleMode::Fixed;
    brush=resolveBrushPreset(preset); resetRasterDabStream(stream);
    dabs=appendRasterDabs(stream,StrokePoint{{32,24}},*brush.brush);
    const auto pixels=projectBrushDabs(dabs,brush.brush->rasterizer);
    RasterLayer layer=makeRasterLayer(49,49); paintRasterSamples(layer,pixels);
    for(int y=0;y<49;++y) for(int x=0;x<49;++x)
        CHECK(rasterLayerPixelAt(layer,{x,y})==rasterLayerPixelAt(layer,{48-x,y}));
    auto &dual=preset.material.dualBrush;
    dual.enabled=true;dual.width=3;dual.height=2;dual.alpha={255,100,20,160,40,220};
    dual.rotationRadians=.3;dual.offsetX=.75;dual.offsetY=.2;
    dual.scatter.enabled=true;dual.scatter.radius=2.4;dual.scatter.count=3;
    brush=resolveBrushPreset(preset,0xFF336699U,17);resetRasterDabStream(stream);
    dabs=appendRasterDabs(stream,StrokePoint{{32,24}},*brush.brush);
    layer=makeRasterLayer(49,49);
    paintRasterSamples(layer,projectBrushDabs(dabs,brush.brush->rasterizer,{},brush.brush->material));
    CHECK(std::ranges::any_of(layer.pixels,[](auto p){return p!=0;}));
    for(int y=0;y<49;++y) for(int x=0;x<49;++x)
        CHECK(rasterLayerPixelAt(layer,{x,y})==rasterLayerPixelAt(layer,{48-x,y}));

    const auto archived=deserializeBrushPreset(serializeBrushPreset(preset));
    CHECK(archived.stroke.symmetry.mode==BrushSymmetryMode::Vertical);
    CHECK(near(restoreBrushPreset(snapshotBrushPreset(preset)).stroke.symmetry.center.x,24));
    DocumentArchive archive;
    archive.brushSources.push_back(snapshotBrushPreset(preset));
    const auto document=deserializeDocumentArchive(serializeDocumentArchive(archive));
    CHECK(document.compatible && document.brushSources.size()==1);
    CHECK(serializeBrushPreset(restoreBrushPreset(document.brushSources.front()))==serializeBrushPreset(preset));
    preset.stroke.symmetry.segments=65;
    CHECK(!resolveBrushPreset(preset).brush);
    preset.stroke.symmetry.segments=3;
    preset.stroke.symmetry.center.x=std::numeric_limits<double>::infinity();
    CHECK(!resolveBrushPreset(preset).brush);

    // Symmetry preserves physical-tip state and never advances Fade for replica stamps.
    preset={};preset.stroke.symmetry.mode=BrushSymmetryMode::Mandala;
    preset.stroke.symmetry.segments=64;preset.stroke.symmetry.center={24,24};
    preset.material.bristle.enabled=true;preset.material.bristle.count=5;
    preset.material.erodible.enabled=true;preset.material.erodible.wear=.3;
    preset.material.airbrushTip.enabled=true;preset.material.airbrushTip.granularity=.4;
    preset.stroke.airbrushEnabled=true;preset.stroke.airbrushRate=2;
    preset.dynamics.sizeVariation.enabled=true;preset.dynamics.sizeVariation.control=BrushDynamicsControl::Fade;
    preset.dynamics.sizeVariation.fadeSteps=4;
    brush=resolveBrushPreset(preset);CHECK(brush.brush);stream={};
    dabs=appendRasterDabs(stream,StrokePoint{{31,26},1,0},*brush.brush);
    CHECK(dabs.size()==128 && stream.sequenceIndex==1);
    for(const auto &d:dabs)CHECK(near(d.tipWear,.3) && near(d.scale,dabs.front().scale));
    CHECK(!projectBrushDabs(dabs,brush.brush->rasterizer,{},brush.brush->material).empty());
    dabs=appendRasterDabs(stream,StrokePoint{{31,26},1,1},*brush.brush);
    CHECK(dabs.size()==256 && stream.sequenceIndex==3);
    const auto sequence=stream.sequenceIndex;
    bool limited=false;
    try { appendRasterDabs(stream,StrokePoint{{31,26},1,300},*brush.brush); }
    catch(const std::length_error &) { limited=true; }
    CHECK(limited && stream.sequenceIndex==sequence && near(stream.previousPoint.time,1));

    // Real application preview, cancel and pixel history cover every reflected area.
    TestDirectory dir("symmetry");
    BitmapPaintSession session;
    CHECK(session.createFile(dir.path/"paint.png",49,49));
    preset={}; preset.size=3; preset.stroke.symmetry.mode=BrushSymmetryMode::DualAxis;
    preset.stroke.symmetry.center={24,24};
    CHECK(session.setBrushPreset(preset));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,{32,30},0)).accepted);
    const auto preview=session.previewPixels().pixels;
    CHECK(rasterLayerPixelAt(session.previewPixels(),{16,18})!=0);
    session.cancelStroke();
    CHECK(std::ranges::all_of(session.previewPixels().pixels,[](auto p){return p==0;}));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,{32,30},1)).accepted);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,{32,30},2)).committed);
    CHECK(session.file().pixels().pixels==preview && session.undo());
    CHECK(std::ranges::all_of(session.file().pixels().pixels,[](auto p){return p==0;}));
    CHECK(session.redo() && session.file().pixels().pixels==preview);
    return 0;
}
