#include <iiPaintEngine>
#include "TestDirectory.h"
#include <algorithm>
#include <iostream>
#include <limits>

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace {
PointerEvent input(PointerEventPhase phase, double x, double y, double time, double pressure = 0.5)
{
    PointerEvent event;
    event.device = PointerDeviceKind::Tablet; event.tool = PointerToolKind::Pen;
    event.phase = phase; event.documentPosition = {x,y}; event.time = time; event.pressure = pressure;
    event.button = phase == PointerEventPhase::Press ? PointerButton::Primary : PointerButton::None;
    event.primaryButtonDown = phase != PointerEventPhase::Release && phase != PointerEventPhase::Cancel;
    return event;
}
std::size_t paintedCount(const RasterLayer &pixels)
{
    return std::ranges::count_if(pixels.pixels,[](std::uint32_t p) { return (p >> 24) != 0; });
}
}
int main()
{
    TestDirectory dir{"session-input"};
    BitmapPaintSession paint,control;
    CHECK(paint.createFile(dir.path / "paint.png",64,64));
    CHECK(control.createFile(dir.path / "control.png",64,64));
    BrushPreset preset; preset.size = 16;
    preset.dynamics.sizeVariation.enabled = true;
    preset.dynamics.sizeVariation.control = BrushDynamicsControl::PenPressure;
    preset.dynamics.sizeVariation.minimum = 0.1;
    CHECK(paint.setBrushPreset(preset) && control.setBrushPreset(preset));
    auto press = input(PointerEventPhase::Press,16,32,0,0.1);
    auto hover = press; hover.hovering = true;
    CHECK(paint.processPointerEvent(hover).accepted && !paint.strokeActive());
    CHECK(paintedCount(paint.previewPixels()) == 0);
    CHECK(paint.processPointerEvent(press).accepted);
    const auto tabletCount = paintedCount(paint.previewPixels());
    CHECK(tabletCount > 0);
    paint.cancelStroke();
    press.device = PointerDeviceKind::Mouse; press.tool = PointerToolKind::Mouse;
    CHECK(paint.processPointerEvent(press).accepted);
    CHECK(paintedCount(paint.previewPixels()) > tabletCount);
    paint.cancelStroke();

    // Selection changes affect the next stroke, including color and eraser mode.
    paint.setBrushColor(0xFFCC2244U); control.setBrushColor(0xFFCC2244U);
    press = input(PointerEventPhase::Press,8,32,1);
    CHECK(paint.processPointerEvent(press).accepted && control.processPointerEvent(press).accepted);
    auto next = preset; next.size = 28; next.material.erodible.enabled = true;
    CHECK(paint.setBrushPreset(next)); paint.setBrushColor(0xFF2244CCU); paint.setEraserMode(true);
    for (int i = 1;i <= 100;++i) {
        const auto event = input(PointerEventPhase::Move,8 + i*0.4,32,1 + i*0.01);
        CHECK(paint.processPointerEvent(event).accepted && control.processPointerEvent(event).accepted);
        CHECK(paint.previewPixels().pixels == control.previewPixels().pixels);
        CHECK(paintedCount(paint.file().pixels()) == 0);
    }
    const auto finalPreview = paint.previewPixels().pixels;
    CHECK(paint.save());
    BitmapFile disk; CHECK(disk.open(dir.path / "paint.png") && paintedCount(disk.pixels()) == 0);
    const auto release = input(PointerEventPhase::Release,48,32,2);
    CHECK(paint.processPointerEvent(release).committed && control.processPointerEvent(release).committed);
    CHECK(paint.file().pixels().pixels == finalPreview && finalPreview == control.file().pixels().pixels);
    CHECK(paint.brushPreset().material.erodible.enabled && paint.brushPreset().material.erodible.wear == 0);

    // Invalid events preserve an active stroke and its pixels.
    paint.setEraserMode(false);
    CHECK(paint.processPointerEvent(input(PointerEventPhase::Press,32,16,3)).accepted);
    const auto pending = paint.previewPixels().pixels;
    auto invalid = input(PointerEventPhase::Move,33,16,3.1);
    invalid.custom[2] = std::numeric_limits<double>::quiet_NaN();
    CHECK(!paint.processPointerEvent(invalid).accepted && paint.strokeActive());
    CHECK(paint.previewPixels().pixels == pending);
    invalid.custom[2] = 0; invalid.time = 2;
    CHECK(!paint.processPointerEvent(invalid).accepted && paint.previewPixels().pixels == pending);
    CHECK(paint.processPointerEvent(input(PointerEventPhase::Cancel,32,16,3.1)).accepted);
    CHECK(!paint.strokeActive() && paint.lastError().empty());
    CHECK(paint.previewPixels().pixels == finalPreview);

    // A protected texture survives preset selection; explicit bypass replaces it.
    auto protectedPreset = preset;
    protectedPreset.stroke.protectTexture = true;
    protectedPreset.material.texture.enabled = true;
    protectedPreset.material.texture.width = protectedPreset.material.texture.height = 2;
    protectedPreset.material.texture.alpha = {0,255,255,0};
    CHECK(paint.setBrushPreset(protectedPreset,false));
    auto replacement = preset;
    replacement.material.texture.enabled = true;
    replacement.material.texture.width = replacement.material.texture.height = 1;
    replacement.material.texture.alpha = {255};
    CHECK(paint.setBrushPreset(replacement));
    CHECK(paint.brushPreset().material.texture.alpha == protectedPreset.material.texture.alpha);
    CHECK(paint.setBrushPreset(replacement,false));
    CHECK(paint.brushPreset().material.texture.alpha == replacement.material.texture.alpha);

    // Undo cancels pending input, and file failures preserve the current pixels/history.
    CHECK(paint.processPointerEvent(input(PointerEventPhase::Press,-2,32,4)).accepted);
    CHECK(paint.undo() && !paint.strokeActive() && paintedCount(paint.file().pixels()) == 0);
    CHECK(paint.redo() && paint.file().pixels().pixels == finalPreview);
    CHECK(!paint.openFile(dir.path / "missing.png") && paint.canUndo());
    CHECK(paint.file().pixels().pixels == finalPreview);
    CHECK(!paint.createFile(dir.path / "invalid.png",0,1) && paint.file().pixels().pixels == finalPreview);
    CHECK(paint.clear(0xFFFFFFFFU) && paint.undo() && paint.file().pixels().pixels == finalPreview);
    CHECK(paint.redo() && rasterLayerPixelAt(paint.file().pixels(),{0,0}) == 0xFFFFFFFFU);
    paint.closeFile();
    CHECK(!paint.processPointerEvent(press).accepted && !paint.file().isOpen());
}
