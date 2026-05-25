#include <cstdint>
#include <vector>

#include "Core/Types.h"
#include "Input/InputStrokeBuilder.h"
#include "Input/PointerEvent.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/StrokeCurve.h"

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

} // namespace

int main()
{
    InputStrokeBuilder builder{};

    const InputStrokeBuildResult ignoredTouch = appendPointerEvent(builder,
                                                                   PointerEvent{
                                                                           PointerDeviceKind::Touch,
                                                                           PointerEventPhase::Press,
                                                                           {1.0, 1.0},
                                                                           1.0,
                                                                           0.0,
                                                                           PointerButton::Primary,
                                                                           true,
                                                                   });
    if (ignoredTouch.strokeCompleted || builder.active || !builder.points.empty()) {
        return 1;
    }

    const InputStrokeBuildResult pressResult = appendPointerEvent(builder,
                                                                  PointerEvent{
                                                                          PointerDeviceKind::Mouse,
                                                                          PointerEventPhase::Press,
                                                                          {2.0, 3.0},
                                                                          0.25,
                                                                          10.0,
                                                                          PointerButton::Primary,
                                                                          true,
                                                                  });
    if (pressResult.strokeCompleted || !builder.active || builder.points.size() != 1) {
        return 1;
    }

    appendPointerEvent(builder,
                       PointerEvent{
                               PointerDeviceKind::Mouse,
                               PointerEventPhase::Move,
                               {5.0, 3.0},
                               0.4,
                               11.0,
                               PointerButton::None,
                               true,
                       });

    const InputStrokeBuildResult releaseResult = appendPointerEvent(builder,
                                                                    PointerEvent{
                                                                            PointerDeviceKind::Mouse,
                                                                            PointerEventPhase::Release,
                                                                            {8.0, 3.0},
                                                                            0.7,
                                                                            12.0,
                                                                            PointerButton::Primary,
                                                                            false,
                                                                    });

    if (!releaseResult.strokeCompleted || builder.active || !builder.points.empty()) {
        return 1;
    }

    if (releaseResult.stroke.points.size() != 3) {
        return 1;
    }

    if (releaseResult.stroke.points.front().position.x != 2.0
            || releaseResult.stroke.points.back().position.x != 8.0) {
        return 1;
    }

    if (releaseResult.stroke.points.front().pressure != 1.0
            || releaseResult.stroke.points.back().pressure != 1.0) {
        return 1;
    }

    StrokeCurve curve = makeStrokeCurve(releaseResult.stroke);
    Rasterizer rasterizer{};
    rasterizer.argb = 0xFF336699U;
    rasterizer.brushWidth = 3;
    rasterizer.brushHeight = 3;
    rasterizer.brushAlpha = {
            0, 255, 0,
            255, 255, 255,
            0, 255, 0,
    };
    rasterizer.spacing = 3.0;
    rasterizer.flow = 0.5;

    const std::vector<RasterSample> samples = rasterizeStrokeCurve(curve, rasterizer);
    if (samples.empty()) {
        return 1;
    }

    constexpr std::uint32_t halfFlowBlue = 0x80336699U;
    if (!hasSampleAt(samples, {2, 3}, halfFlowBlue)
            || !hasSampleAt(samples, {5, 3}, halfFlowBlue)
            || !hasSampleAt(samples, {8, 3}, halfFlowBlue)) {
        return 1;
    }

    if (hasSampleAt(samples, {1, 2}, halfFlowBlue)) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(12, 8);
    paintRasterSamples(layer, samples);
    if (rasterLayerPixelAt(layer, {5, 3}) != halfFlowBlue) {
        return 1;
    }

    return 0;
}
