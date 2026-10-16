#include "Application/BitmapPaintSession.h"
#include "Brush/BrushLibrary.h"
#include "Brush/BrushPresetSerializer.h"
#include <iostream>
#include <limits>
#include "TestDirectory.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
PointerEvent event(PointerEventPhase phase, double x, double y, double time)
{
    PointerEvent p;
    p.phase = phase; p.documentPosition = {x,y}; p.time = time;
    p.button = phase == PointerEventPhase::Press ? PointerButton::Primary : PointerButton::None;
    p.primaryButtonDown = phase != PointerEventPhase::Release;
    return p;
}
int main()
{
    TestDirectory dir{"bitmap-session"};
    BitmapPaintSession session;
    CHECK(session.createFile(dir.path / "paint.png",64,64));
    BrushPreset brush; brush.size = 12; brush.flow = .3F;
    brush.stroke.noiseEnabled = true; brush.material.erodible.enabled = true;
    CHECK(session.setBrushPresetData(serializeBrushPreset(brush)));
    const auto original = session.file().pixels().pixels;
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,16,32,0)).accepted);
    CHECK(session.strokeActive());
    CHECK(session.processPointerEvent(event(PointerEventPhase::Move,48,32,1)).accepted);
    CHECK(session.file().pixels().pixels == original);
    const auto preview = session.previewPixels().pixels;
    CHECK(preview != original);
    const auto release = session.processPointerEvent(event(PointerEventPhase::Release,48,32,1));
    CHECK(release.accepted && release.committed && !session.strokeActive());
    CHECK(session.file().pixels().pixels == preview);
    CHECK(session.brushPreset().material.erodible.wear > 0);
    CHECK(session.undo() && session.file().pixels().pixels == original);
    CHECK(session.redo() && session.file().pixels().pixels == preview);
    CHECK(session.save());
    BitmapFile loaded; CHECK(loaded.open(dir.path / "paint.png") && loaded.pixels().pixels == preview);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,16,16,2)).accepted);
    const auto wear = session.brushPreset().material.erodible.wear;
    CHECK(session.processPointerEvent(event(PointerEventPhase::Cancel,16,16,2)).accepted);
    CHECK(session.file().pixels().pixels == preview && session.brushPreset().material.erodible.wear == wear);
    auto invalid = brush; invalid.size = std::numeric_limits<float>::quiet_NaN();
    const auto before = session.brushPresetData();
    CHECK(!session.setBrushPreset(invalid) && session.brushPresetData() == before);
    auto air = builtInBrushPresets()[2]; air.stroke.airbrushRate = 20;
    CHECK(session.setBrushPreset(air));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,32,8,3)).accepted);
    const auto first = session.previewPixels().pixels;
    CHECK(session.advanceTime(3.5).accepted && session.previewPixels().pixels != first);
    auto marker = builtInBrushPresets()[3];
    CHECK(session.setBrushPreset(marker));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,32,8,3.5)).committed);
    CHECK(session.brushPreset().name == marker.name);
    auto badEvent = event(PointerEventPhase::Press,0,0,4);
    badEvent.documentPosition.x = std::numeric_limits<double>::infinity();
    CHECK(!session.processPointerEvent(badEvent).accepted && !session.strokeActive());
    session.setEraserMode(true);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,32,32,4)).accepted);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,32,32,4)).committed);
    CHECK(session.undo() && session.redo());
    return 0;
}
