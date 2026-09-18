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

} // namespace

InputStrokeBuildResult appendPointerEvent(InputStrokeBuilder &builder, const PointerEvent &event)
{
    InputStrokeBuildResult result;

    if (!isStrokeEvent(event) || event.hovering) {
        return result;
    }

    if (event.phase == PointerEventPhase::Cancel) {
        resetInputStrokeBuilder(builder);
        result.strokeCancelled = true;
        return result;
    }

    if (isPrimaryPress(event) || (!builder.active && isTabletContactMove(event))) {
        resetInputStrokeBuilder(builder);
        builder.active = true;
        result.pointAvailable = true;
        result.strokeStarted = true;
        result.point = makeStrokePoint(event);
        return result;
    }

    if (!builder.active) {
        return result;
    }

    if (event.phase == PointerEventPhase::Move) {
        if (event.primaryButtonDown) {
            result.pointAvailable = true;
            result.point = makeStrokePoint(event);
        }
        return result;
    }

    if (event.phase == PointerEventPhase::Release) {
        result.pointAvailable = true;
        result.point = makeStrokePoint(event);
        result.strokeCompleted = true;
        resetInputStrokeBuilder(builder);
    }

    return result;
}

void resetInputStrokeBuilder(InputStrokeBuilder &builder)
{
    builder.active = false;
}
