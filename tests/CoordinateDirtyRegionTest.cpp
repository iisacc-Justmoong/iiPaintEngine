#include <cmath>
#include <vector>

#include "Canvas/CanvasViewport.h"
#include "Input/InputStrokeBuilder.h"
#include "Render/DirtyRegion.h"
#include "Stroke/StrokeCommand.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

bool contains(DevicePixelRect rect, DevicePixelPoint point)
{
    return point.x >= rect.origin.x
            && point.y >= rect.origin.y
            && point.x < rect.origin.x + rect.width
            && point.y < rect.origin.y + rect.height;
}

PointerEvent mouseEvent(PointerEventPhase phase,
                        DocumentPoint documentPosition,
                        Types::Scalar time,
                        PointerButton button,
                        bool down)
{
    return PointerEvent{
            PointerDeviceKind::Mouse,
            phase,
            documentPosition,
            1.0,
            time,
            button,
            down,
    };
}

} // namespace

int main()
{
    CanvasViewport viewport{};
    viewport.documentRect = {{100.0, 50.0}, 200.0, 100.0};
    viewport.viewRect = {{0.0, 0.0}, 400.0, 200.0};
    viewport.devicePixelRect = {{0, 0}, 400, 200};
    viewport.zoom = 2.0;
    viewport.devicePixelRatio = 1.0;

    const DocumentPoint documentPoint = documentPointFromViewPoint(viewport, ViewPoint{20.0, 10.0});
    if (!nearlyEqual(documentPoint.x, 110.0) || !nearlyEqual(documentPoint.y, 55.0)) {
        return 1;
    }

    const ViewPoint viewPoint = viewPointFromDocumentPoint(viewport, documentPoint);
    const DevicePixelPoint devicePoint = devicePixelPointFromDocumentPoint(viewport, documentPoint);
    if (!nearlyEqual(viewPoint.x, 20.0)
            || !nearlyEqual(viewPoint.y, 10.0)
            || devicePoint.x != 20
            || devicePoint.y != 10) {
        return 1;
    }

    InputStrokeBuilder builder{};
    appendPointerEvent(builder, mouseEvent(PointerEventPhase::Press,
                                           documentPoint,
                                           0.0,
                                           PointerButton::Primary,
                                           true));
    const InputStrokeBuildResult result = appendPointerEvent(builder,
                                                             mouseEvent(PointerEventPhase::Release,
                                                                        {112.0, 55.0},
                                                                        1.0,
                                                                        PointerButton::Primary,
                                                                        false));
    if (!result.strokeCompleted
            || !nearlyEqual(result.stroke.points.front().position.x, 110.0)
            || !nearlyEqual(result.stroke.points.back().position.x, 112.0)) {
        return 1;
    }

    BrushState brush{};
    brush.rasterizer.brushWidth = 3;
    brush.rasterizer.brushHeight = 3;
    brush.rasterizer.brushAlpha = {
            255, 255, 255,
            255, 255, 255,
            255, 255, 255,
    };
    brush.rasterizer.spacing = 2.0;
    brush.rasterizer.flow = 1.0;
    brush.rasterizer.opacity = 1.0;

    const StrokeCommand command = makeStrokeCommand(result.stroke, brush, Stabilizer{0.0});
    if (command.dabs.empty()
            || command.dabDirtyBounds.size() != command.dabs.size()
            || command.dirtyBounds.width <= 0.0
            || command.dirtyBounds.origin.x < 100.0) {
        return 1;
    }

    const RasterProjection projection{
            viewport.documentRect.origin,
            viewport.devicePixelRect.origin,
            viewport.zoom * viewport.devicePixelRatio,
    };
    const std::vector<RasterSample> samples = projectBrushDabs(command.dabs,
                                                               command.brush.rasterizer,
                                                               projection);
    bool foundProjectedSample = false;
    for (const RasterSample &sample : samples) {
        if (sample.position.x == 20 && sample.position.y == 10) {
            foundProjectedSample = true;
        }
    }
    if (!foundProjectedSample) {
        return 1;
    }

    const std::vector<DevicePixelRect> dabBounds = deviceBoundsForBrushDabs(command.dabs,
                                                                            command.brush.rasterizer,
                                                                            projection);
    const DirtyRegion dirtyRegion = makeDirtyRegion(dabBounds);
    if (dirtyRegion.rects.size() != command.dabs.size()
            || dirtyRegion.bounds.width >= viewport.devicePixelRect.width
            || !contains(dirtyRegion.bounds, {20, 10})) {
        return 1;
    }

    return 0;
}
