#include <cstdint>
#include <vector>

#include "Input/InputStrokeBuilder.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

namespace {

bool hasSampleAt(const std::vector<RasterSample> &samples, DevicePixelPoint position, std::uint32_t argb)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y && sample.argb == argb) {
            return true;
        }
    }
    return false;
}

PointerEvent pointerEvent(PointerDeviceKind device,
                          PointerEventPhase phase,
                          DocumentPoint position,
                          Types::Scalar pressure,
                          Types::Scalar time,
                          PointerButton button,
                          bool down)
{
    return PointerEvent{device, phase, position, pressure, time, button, down};
}

} // namespace

int main()
{
    InputStrokeBuilder builder{};
    const InputStrokeBuildResult ignoredTouch = appendPointerEvent(
            builder,
            pointerEvent(PointerDeviceKind::Touch,
                         PointerEventPhase::Press,
                         {1.0, 1.0},
                         1.0,
                         0.0,
                         PointerButton::Primary,
                         true));
    if (ignoredTouch.pointAvailable || ignoredTouch.strokeCompleted || builder.active) {
        return 1;
    }

    BrushState brush{};
    brush.rasterizer.argb = 0xFF336699U;
    brush.rasterizer.brushWidth = 3;
    brush.rasterizer.brushHeight = 3;
    brush.rasterizer.brushAlpha = {0, 255, 0, 255, 255, 255, 0, 255, 0};
    brush.rasterizer.spacing = 3.0;
    brush.rasterizer.flow = 0.5;
    RasterDabStream stream{};
    std::vector<RasterSample> pixels;

    const PointerEvent events[]{
            pointerEvent(PointerDeviceKind::Mouse,
                         PointerEventPhase::Press,
                         {2.0, 3.0},
                         0.25,
                         10.0,
                         PointerButton::Primary,
                         true),
            pointerEvent(PointerDeviceKind::Mouse,
                         PointerEventPhase::Move,
                         {5.0, 3.0},
                         0.4,
                         11.0,
                         PointerButton::None,
                         true),
            pointerEvent(PointerDeviceKind::Mouse,
                         PointerEventPhase::Release,
                         {8.0, 3.0},
                         0.7,
                         12.0,
                         PointerButton::Primary,
                         false),
    };
    for (std::size_t index = 0; index < std::size(events); ++index) {
        const InputStrokeBuildResult result = appendPointerEvent(builder, events[index]);
        if (!result.pointAvailable || result.point.pressure != 1.0) {
            return 1;
        }
        std::vector<RasterSample> eventPixels = projectBrushDabs(
                appendRasterDabs(stream, result.point, brush, result.strokeCompleted),
                brush.rasterizer);
        pixels.insert(pixels.end(), eventPixels.begin(), eventPixels.end());
    }

    constexpr std::uint32_t halfFlowBlue = 0x80336699U;
    if (builder.active
            || stream.active
            || !hasSampleAt(pixels, {2, 3}, halfFlowBlue)
            || !hasSampleAt(pixels, {5, 3}, halfFlowBlue)
            || !hasSampleAt(pixels, {8, 3}, halfFlowBlue)
            || hasSampleAt(pixels, {1, 2}, halfFlowBlue)) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(12, 8);
    paintRasterSamples(layer, pixels);
    return rasterLayerPixelAt(layer, {5, 3}) == halfFlowBlue ? 0 : 1;
}
