//
// Created by Justmoong on 2026 May 24.
//

#include "InputStrokeBuilder.h"

namespace {

bool isMouseEvent(const PointerEvent &event)
{
    return event.device == PointerDeviceKind::Mouse;
}

bool isPrimaryPress(const PointerEvent &event)
{
    return event.phase == PointerEventPhase::Press
            && event.button == PointerButton::Primary
            && event.primaryButtonDown;
}

StrokePoint makeMouseStrokePoint(const PointerEvent &event)
{
    return StrokePoint{
            event.canvasPosition,
            1.0,
            event.time,
            0.0,
            event.tiltX,
            event.tiltY,
    };
}

void appendDistinctPoint(InputStrokeBuilder &builder, const PointerEvent &event)
{
    const StrokePoint point = makeMouseStrokePoint(event);
    if (!builder.points.empty()) {
        const StrokePoint &last = builder.points.back();
        if (last.position.x == point.position.x
                && last.position.y == point.position.y
                && last.time == point.time) {
            return;
        }
    }

    builder.points.push_back(point);
}

} // namespace

InputStrokeBuildResult appendPointerEvent(InputStrokeBuilder &builder, const PointerEvent &event)
{
    InputStrokeBuildResult result;

    if (!isMouseEvent(event)) {
        return result;
    }

    if (event.phase == PointerEventPhase::Cancel) {
        resetInputStrokeBuilder(builder);
        return result;
    }

    if (isPrimaryPress(event)) {
        resetInputStrokeBuilder(builder);
        builder.active = true;
        appendDistinctPoint(builder, event);
        return result;
    }

    if (!builder.active) {
        return result;
    }

    if (event.phase == PointerEventPhase::Move) {
        if (event.primaryButtonDown) {
            appendDistinctPoint(builder, event);
        }
        return result;
    }

    if (event.phase == PointerEventPhase::Release) {
        appendDistinctPoint(builder, event);
        result.strokeCompleted = !builder.points.empty();
        result.stroke.points = builder.points;
        resetInputStrokeBuilder(builder);
    }

    return result;
}

void resetInputStrokeBuilder(InputStrokeBuilder &builder)
{
    builder.active = false;
    builder.points.clear();
}
