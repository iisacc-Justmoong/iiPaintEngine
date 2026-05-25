#include <cstdint>

#include "Input/InputStrokeBuilder.h"
#include "Input/PointerEvent.h"
#include "Layer/RasterLayer.h"
#include "Stroke/LiveStroke.h"

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

PointerEvent mouseEvent(PointerEventPhase phase,
                        CanvasPoint position,
                        Types::Scalar time,
                        PointerButton button,
                        bool down)
{
    return PointerEvent{
            PointerDeviceKind::Mouse,
            phase,
            position,
            1.0,
            time,
            button,
            down,
    };
}

} // namespace

int main()
{
    InputStrokeBuilder builder{};
    appendPointerEvent(builder, mouseEvent(PointerEventPhase::Press,
                                           {0.0, 0.0},
                                           0.0,
                                           PointerButton::Primary,
                                           true));
    appendPointerEvent(builder, mouseEvent(PointerEventPhase::Move,
                                           {5.0, 9.0},
                                           1.0,
                                           PointerButton::None,
                                           true));
    const InputStrokeBuildResult moveResult = appendPointerEvent(builder,
                                                                 mouseEvent(PointerEventPhase::Move,
                                                                            {10.0, 0.0},
                                                                            2.0,
                                                                            PointerButton::None,
                                                                            true));
    if (moveResult.strokeCompleted || !builder.active) {
        return 1;
    }

    BrushState brush{};
    brush.randomSeed = 23;
    brush.rasterizer.radius = 1;
    brush.rasterizer.flow = 1.0;
    brush.rasterizer.opacity = 1.0;
    brush.rasterizer.spacing = 1.0;

    LiveStrokeBuffer liveBuffer{};
    updateLiveStrokeBuffer(liveBuffer, activeStrokeInput(builder), brush, Stabilizer{1.0});
    if (!liveBuffer.active
            || !liveBuffer.frame.active
            || liveBuffer.frame.samples.empty()
            || liveBuffer.frame.dirtyBounds.width <= 0
            || liveBuffer.frame.dirtyBounds.height <= 0) {
        return 1;
    }

    const StrokePoint &rawMiddle = liveBuffer.frame.rawInput.points[1];
    const StrokePoint &displayedMiddle = liveBuffer.frame.displayedInput.points[1];
    const StrokePoint &rawTip = liveBuffer.frame.rawInput.points.back();
    const StrokePoint &displayedTip = liveBuffer.frame.displayedInput.points.back();

    if (displayedMiddle.position.y >= rawMiddle.position.y
            || displayedTip.position.x != rawTip.position.x
            || displayedTip.position.y != rawTip.position.y
            || displayedTip.time != rawTip.time) {
        return 1;
    }

    RasterLayer committed = makeRasterLayer(16, 16);
    RasterLayer live = makeRasterLayer(16, 16);
    paintRasterSamples(live, liveBuffer.frame.samples);
    if (alphaOf(rasterLayerPixelAt(live, {10, 0})) == 0
            || rasterLayerPixelAt(committed, {10, 0}) != 0x00000000U) {
        return 1;
    }

    const InputStrokeBuildResult releaseResult = appendPointerEvent(builder,
                                                                    mouseEvent(PointerEventPhase::Release,
                                                                               {10.0, 0.0},
                                                                               3.0,
                                                                               PointerButton::Primary,
                                                                               false));
    if (!releaseResult.strokeCompleted || builder.active) {
        return 1;
    }

    const StrokeCommand command = makeStrokeCommand(releaseResult.stroke, brush, Stabilizer{1.0});
    paintRasterSamples(committed, projectBrushDabs(command.dabs, command.brush.rasterizer));
    clearLiveStrokeBuffer(liveBuffer);

    if (liveBuffer.active
            || alphaOf(rasterLayerPixelAt(committed, {10, 0})) == 0) {
        return 1;
    }

    return 0;
}
