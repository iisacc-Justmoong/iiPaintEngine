#include <cmath>
#include <iostream>
#include <limits>
#include "iiPaintEngine"
#include "TestDirectory.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace { bool near(double a,double b) {return std::abs(a-b)<1e-7;} }
int main()
{
    BrushPreset p; p.size=2; p.stroke.spacing=1;
    p.stroke.smoothingEnabled=true; p.stroke.smoothing=1;
    p.stroke.smoothingMode=BrushSmoothingMode::PulledString; p.stroke.smoothingRadius=5;
    auto resolved=resolveBrushPreset(p);
    CHECK(resolved.brush);
    RasterDabStream coarse,fine;
    appendRasterDabs(coarse,StrokePoint{{0,0},1,0},*resolved.brush);
    CHECK(appendRasterDabs(coarse,StrokePoint{{3,0},1,.1},*resolved.brush).empty());
    CHECK(near(coarse.previousPoint.position.x,0));
    appendRasterDabs(coarse,StrokePoint{{20,0},1,1},*resolved.brush);
    CHECK(near(coarse.previousPoint.position.x,15));
    appendRasterDabs(fine,StrokePoint{{0,0},1,0},*resolved.brush);
    for(int i=1;i<=20;++i) appendRasterDabs(fine,StrokePoint{{double(i),0},1,i/20.0},*resolved.brush);
    CHECK(near(fine.previousPoint.position.x,coarse.previousPoint.position.x));
    CHECK(fine.sequenceIndex==coarse.sequenceIndex);
    p.stroke.smoothingCatchUpOnEnd=true; resolved=resolveBrushPreset(p);
    const auto end=appendRasterDabs(coarse,StrokePoint{{20,0},1,2},*resolved.brush,true);
    CHECK(!end.empty() && near(end.back().position.x,20) && !coarse.active);
    const auto stored=deserializeBrushPreset(serializeBrushPreset(p));
    CHECK(stored.stroke.smoothingMode==BrushSmoothingMode::PulledString);
    CHECK(stored.stroke.smoothingCatchUpOnEnd && near(stored.stroke.smoothingRadius,5));
    CHECK(serializeBrushPreset(restoreBrushPreset(snapshotBrushPreset(p)))==serializeBrushPreset(p));
    for(int version:{1,2,3}) {
        const auto old=readBrushPreset("formatMagic\t\"iiPaintBrushPreset\"\nformatVersion\t"+std::to_string(version)
                +"\nsize\t12\nflow\t1\nopacity\t1\nhardness\t1\ndensity\t1\n");
        CHECK(old.preset && old.preset->stroke.symmetry.mode==BrushSymmetryMode::None);
        CHECK(old.preset->stroke.smoothingMode==BrushSmoothingMode::Exponential);
        CHECK(!old.preset->stroke.smoothingCatchUp && !old.preset->stroke.smoothingCatchUpOnEnd);
    }

    // Clock-driven catch-up is invariant to splitting a stationary time interval.
    p.stroke.smoothingCatchUp=true; p.stroke.smoothingCatchUpRate=2;
    resolved=resolveBrushPreset(p); coarse={}; fine={};
    for(auto *s:{&coarse,&fine}) {
        appendRasterDabs(*s,StrokePoint{{0,0},1,0},*resolved.brush);
        appendRasterDabs(*s,StrokePoint{{20,0},1,1},*resolved.brush);
    }
    appendRasterDabs(coarse,StrokePoint{{20,0},1,2},*resolved.brush);
    appendRasterDabs(fine,StrokePoint{{20,0},1,1.5},*resolved.brush);
    appendRasterDabs(fine,StrokePoint{{20,0},1,2},*resolved.brush);
    CHECK(near(coarse.previousPoint.position.x,fine.previousPoint.position.x));
    CHECK(coarse.previousPoint.position.x>19 && coarse.previousPoint.position.x<20);
    const auto unchanged=coarse.sequenceIndex;
    bool rejected=false;
    try { appendRasterDabs(coarse,StrokePoint{{20,0},1,1},*resolved.brush); }
    catch(const std::invalid_argument &) { rejected=true; }
    CHECK(rejected && coarse.sequenceIndex==unchanged);
    p.stroke.smoothingRadius=std::numeric_limits<double>::quiet_NaN();
    CHECK(!resolveBrushPreset(p).brush);
    p.stroke.smoothingRadius=5;
    p.stroke.smoothingCatchUpRate=0;
    CHECK(!resolveBrushPreset(p).brush);
    p.stroke.smoothingCatchUpRate=2;

    TestDirectory dir("stabilization"); BitmapPaintSession paint;
    CHECK(paint.createFile(dir.path/"stroke.png",32,8));
    CHECK(paint.setBrushPreset(p));
    PointerEvent e; e.phase=PointerEventPhase::Press; e.button=PointerButton::Primary;
    e.primaryButtonDown=true; e.documentPosition={2,4}; e.time=0;
    CHECK(paint.processPointerEvent(e).accepted);
    e.phase=PointerEventPhase::Move; e.documentPosition={22,4}; e.time=1;
    CHECK(paint.processPointerEvent(e).accepted);
    CHECK(rasterLayerPixelAt(paint.previewPixels(),{20,4})==0);
    CHECK(paint.advanceTime(2).accepted);
    CHECK(rasterLayerPixelAt(paint.previewPixels(),{20,4})!=0);
    e.phase=PointerEventPhase::Release; e.primaryButtonDown=false; e.time=3;
    CHECK(paint.processPointerEvent(e).committed);
    CHECK(paint.undo() && paint.redo());
    return 0;
}
