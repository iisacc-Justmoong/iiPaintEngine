#include <cstdint>

#include "Document/PaintDocument.h"
#include "Input/InputStrokeBuilder.h"
#include "Layer/DrawingSurface.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

namespace {

PointerEvent mouseEvent(PointerEventPhase phase,
                        DocumentPoint position,
                        Types::Scalar time,
                        PointerButton button,
                        bool down)
{
    return PointerEvent{PointerDeviceKind::Mouse, phase, position, 1.0, time, button, down};
}

} // namespace

int main()
{
    InputStrokeBuilder input{};
    RasterDabStream dabs{};
    BrushState brush{};
    brush.rasterizer.argb = 0xFF000000U;
    brush.rasterizer.radius = 1;
    brush.rasterizer.spacing = 1.0;
    RasterLayer layer = makeRasterLayer(16, 16);

    const PointerEvent events[]{
            mouseEvent(PointerEventPhase::Press, {4.0, 4.0}, 0.0, PointerButton::Primary, true),
            mouseEvent(PointerEventPhase::Move, {5.0, 7.0}, 1.0, PointerButton::None, true),
            mouseEvent(PointerEventPhase::Release, {6.0, 4.0}, 2.0, PointerButton::Primary, false),
    };
    for (const PointerEvent &event : events) {
        const InputStrokeBuildResult result = appendPointerEvent(input, event);
        if (!result.pointAvailable) {
            return 1;
        }
        const std::vector<BrushDab> eventDabs = appendRasterDabs(dabs,
                                                                 result.point,
                                                                 brush,
                                                                 result.strokeCompleted);
        paintRasterSamples(layer, projectBrushDabs(eventDabs, brush.rasterizer));
    }

    if (input.active || dabs.active || rasterLayerPixelAt(layer, {4, 4}) != 0xFF000000U) {
        return 1;
    }

    PaintDocument document = makePaintDocument(layer);
    if (document.layers.layers.size() != 1
            || drawingSurfacePixelAt(document.layers.layers.front().surface, {4, 4})
                    != 0xFF000000U) {
        return 1;
    }
    return 0;
}
