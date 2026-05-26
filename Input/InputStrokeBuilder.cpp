//
// Created by Justmoong on 2026 May 24.
//

#include "InputStrokeBuilder.h"

namespace {

bool isMouseEvent(const PointerEvent &event)
{
    return event.device == PointerDeviceKind::Mouse;
}

bool isTabletEvent(const PointerEvent &event)
{
    return event.device == PointerDeviceKind::Tablet;
}

bool isStrokeEvent(const PointerEvent &event)
{
    return isMouseEvent(event) || isTabletEvent(event);
}

bool isPrimaryPress(const PointerEvent &event)
{
    return event.phase == PointerEventPhase::Press
            && (event.button == PointerButton::Primary || event.button == PointerButton::Eraser)
            && event.primaryButtonDown;
}

bool isTabletContactMove(const PointerEvent &event)
{
    return isTabletEvent(event)
            && event.phase == PointerEventPhase::Move
            && (event.button == PointerButton::Primary || event.button == PointerButton::Eraser)
            && event.primaryButtonDown;
}

std::uint32_t deviceStateFromEvent(const PointerEvent &event)
{
    if (event.deviceState != 0) {
        return event.deviceState;
    }

    std::uint32_t state = 0;
    if (event.primaryButtonDown) {
        state |= PointerDeviceStatePrimaryButton;
    }
    if (event.barrelButtonDown) {
        state |= PointerDeviceStateBarrelButton;
    }
    if (event.eraserActive) {
        state |= PointerDeviceStateEraser;
    }
    if (event.hovering) {
        state |= PointerDeviceStateHover;
    }
    return state;
}

StrokePoint makeStrokePoint(const PointerEvent &event)
{
    return StrokePoint{
            event.documentPosition,
            isMouseEvent(event) ? 1.0 : event.pressure,
            event.time,
            0.0,
            event.tiltX,
            event.tiltY,
            deviceStateFromEvent(event),
            0.0,
            event.rotationRadians,
    };
}

void appendDistinctPoint(InputStrokeBuilder &builder, const PointerEvent &event)
{
    const StrokePoint point = makeStrokePoint(event);
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

    if (!isStrokeEvent(event) || event.hovering) {
        return result;
    }

    if (event.phase == PointerEventPhase::Cancel) {
        resetInputStrokeBuilder(builder);
        return result;
    }

    if (isPrimaryPress(event) || (!builder.active && isTabletContactMove(event))) {
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

StrokeInput activeStrokeInput(const InputStrokeBuilder &builder)
{
    StrokeInput input;
    if (builder.active) {
        input.points = builder.points;
    }
    return input;
}

void resetInputStrokeBuilder(InputStrokeBuilder &builder)
{
    builder.active = false;
    builder.points.clear();
}
