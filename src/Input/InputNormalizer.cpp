//
// Created by Justmoong on 2026 May 24.
//

#include "InputNormalizer.h"

#include <algorithm>

#include "Input/PressureInput.h"

namespace {

Types::Scalar clampTilt(Types::Scalar value, Types::Scalar maxTilt)
{
    const Types::Scalar limit = std::max<Types::Scalar>(0.0, maxTilt);
    return std::clamp(value, -limit, limit);
}

Types::Scalar calibratedTiltX(const InputNormalizer &normalizer, const TabletState &tablet)
{
    if (!normalizer.tiltEnabled) {
        return 0.0;
    }
    const TabletTiltCalibration &calibration = normalizer.tabletTiltCalibration;
    return clampTilt((tablet.tiltX + calibration.offsetX) * calibration.scaleX, calibration.maxTilt);
}

Types::Scalar calibratedTiltY(const InputNormalizer &normalizer, const TabletState &tablet)
{
    if (!normalizer.tiltEnabled) {
        return 0.0;
    }
    const TabletTiltCalibration &calibration = normalizer.tabletTiltCalibration;
    return clampTilt((tablet.tiltY + calibration.offsetY) * calibration.scaleY, calibration.maxTilt);
}

bool isEraser(const InputNormalizer &normalizer, const TabletState &tablet)
{
    if (!normalizer.eraserEnabled) {
        return false;
    }
    return tablet.eraser || tablet.tool == TabletToolKind::Eraser;
}

bool isHovering(const InputNormalizer &normalizer,
                const TabletState &tablet,
                PointerEventPhase phase)
{
    return normalizer.hoverEnabled
            && phase == PointerEventPhase::Move
            && (tablet.hovering || (tablet.inProximity && !tablet.contact));
}

PointerButton tabletButton(const InputNormalizer &normalizer, const TabletState &tablet)
{
    if (isEraser(normalizer, tablet)) {
        return PointerButton::Eraser;
    }
    if (tablet.contact || tablet.primaryButtonDown) {
        return PointerButton::Primary;
    }
    if (normalizer.barrelButtonEnabled && tablet.barrelButtonDown) {
        return PointerButton::Barrel;
    }
    return PointerButton::None;
}

std::uint32_t tabletDeviceState(const InputNormalizer &normalizer,
                                const TabletState &tablet,
                                PointerEventPhase phase)
{
    std::uint32_t state = 0;
    if (tablet.contact || tablet.primaryButtonDown) {
        state |= PointerDeviceStatePrimaryButton;
    }
    if (normalizer.barrelButtonEnabled && tablet.barrelButtonDown) {
        state |= PointerDeviceStateBarrelButton;
    }
    if (isEraser(normalizer, tablet)) {
        state |= PointerDeviceStateEraser;
    }
    if (isHovering(normalizer, tablet, phase)) {
        state |= PointerDeviceStateHover;
    }
    return state;
}

PointerEventPhase gesturePointerPhase(TouchGesturePhase phase)
{
    switch (phase) {
        case TouchGesturePhase::Begin:
            return PointerEventPhase::Press;
        case TouchGesturePhase::End:
            return PointerEventPhase::Release;
        case TouchGesturePhase::Cancel:
            return PointerEventPhase::Cancel;
        case TouchGesturePhase::None:
        case TouchGesturePhase::Update:
            return PointerEventPhase::Move;
    }
    return PointerEventPhase::Move;
}

} // namespace

PointerEvent normalizeTabletPointerEvent(const InputNormalizer &normalizer,
                                         const TabletState &tablet,
                                         PointerEventPhase phase)
{
    PointerEvent event;
    event.device = PointerDeviceKind::Tablet;
    event.phase = phase;
    event.documentPosition = tablet.documentPosition;
    event.pressure = resolvePressureInput(PressureInput{
            normalizer.pressureEnabled,
            normalizer.pressureMin,
            normalizer.pressureMax,
            tablet.pressure,
            tablet.contact,
            tablet.hovering,
            normalizer.pressureCurveMinimum,
            normalizer.pressureCurveCenter,
            normalizer.pressureCurveMaximum,
    });
    event.time = tablet.time;
    event.button = tabletButton(normalizer, tablet);
    event.hovering = isHovering(normalizer, tablet, phase);
    event.primaryButtonDown = tablet.contact && !event.hovering;
    event.tiltX = calibratedTiltX(normalizer, tablet);
    event.tiltY = calibratedTiltY(normalizer, tablet);
    event.tool = isEraser(normalizer, tablet) ? PointerToolKind::Eraser : PointerToolKind::Pen;
    event.barrelButtonDown = normalizer.barrelButtonEnabled && tablet.barrelButtonDown;
    event.eraserActive = isEraser(normalizer, tablet);
    event.rotationRadians = normalizer.rotationEnabled ? tablet.rotationRadians : 0.0;
    event.deviceState = tabletDeviceState(normalizer, tablet, phase);
    return event;
}

PointerEvent normalizeTouchGesturePointerEvent(const InputNormalizer &normalizer, const TouchGestureState &gesture)
{
    PointerEvent event;
    event.device = PointerDeviceKind::Touch;
    event.phase = gesturePointerPhase(gesture.phase);
    event.documentPosition = gesture.centroid;
    event.pressure = 0.0;
    event.time = gesture.time;
    event.button = PointerButton::None;
    event.primaryButtonDown = false;
    event.tool = PointerToolKind::Finger;
    if (!normalizer.touchGestureEnabled) {
        return event;
    }
    event.gesturePhase = gesture.phase;
    event.gestureCentroid = gesture.centroid;
    event.gestureTranslation = gesture.translation;
    event.gestureScale = gesture.scale;
    event.gestureRotationRadians = gesture.rotationRadians;
    event.touchPointCount = gesture.fingerCount;
    if (gesture.phase != TouchGesturePhase::None) {
        event.deviceState = PointerDeviceStateTouchGesture;
    }
    return event;
}
