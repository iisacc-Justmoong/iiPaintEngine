//
// Created by Justmoong on 2026 May 24.
//

#include "InputNormalizer.h"

#include <algorithm>

namespace {

Types::Scalar clamp01(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Scalar clampTilt(Types::Scalar value, Types::Scalar maxTilt)
{
    const Types::Scalar limit = std::max<Types::Scalar>(0.0, maxTilt);
    return std::clamp(value, -limit, limit);
}

Types::Scalar normalizePressure(const InputNormalizer &normalizer,
                                const TabletState &tablet)
{
    if (!tablet.contact || tablet.hovering) {
        return 0.0;
    }

    const Types::Scalar range = normalizer.pressureMax - normalizer.pressureMin;
    if (range <= 0.0) {
        return clamp01(tablet.pressure);
    }
    return clamp01((tablet.pressure - normalizer.pressureMin) / range);
}

Types::Scalar calibratedTiltX(const InputNormalizer &normalizer, const TabletState &tablet)
{
    const TabletTiltCalibration &calibration = normalizer.tabletTiltCalibration;
    return clampTilt((tablet.tiltX + calibration.offsetX) * calibration.scaleX, calibration.maxTilt);
}

Types::Scalar calibratedTiltY(const InputNormalizer &normalizer, const TabletState &tablet)
{
    const TabletTiltCalibration &calibration = normalizer.tabletTiltCalibration;
    return clampTilt((tablet.tiltY + calibration.offsetY) * calibration.scaleY, calibration.maxTilt);
}

bool isEraser(const TabletState &tablet)
{
    return tablet.eraser || tablet.tool == TabletToolKind::Eraser;
}

PointerButton tabletButton(const TabletState &tablet)
{
    if (isEraser(tablet)) {
        return PointerButton::Eraser;
    }
    if (tablet.contact || tablet.primaryButtonDown) {
        return PointerButton::Primary;
    }
    if (tablet.barrelButtonDown) {
        return PointerButton::Barrel;
    }
    return PointerButton::None;
}

std::uint32_t tabletDeviceState(const TabletState &tablet)
{
    std::uint32_t state = 0;
    if (tablet.contact || tablet.primaryButtonDown) {
        state |= PointerDeviceStatePrimaryButton;
    }
    if (tablet.barrelButtonDown) {
        state |= PointerDeviceStateBarrelButton;
    }
    if (isEraser(tablet)) {
        state |= PointerDeviceStateEraser;
    }
    if (tablet.hovering || (tablet.inProximity && !tablet.contact)) {
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
    event.pressure = normalizePressure(normalizer, tablet);
    event.time = tablet.time;
    event.button = tabletButton(tablet);
    event.primaryButtonDown = tablet.contact && !tablet.hovering;
    event.tiltX = calibratedTiltX(normalizer, tablet);
    event.tiltY = calibratedTiltY(normalizer, tablet);
    event.tool = isEraser(tablet) ? PointerToolKind::Eraser : PointerToolKind::Pen;
    event.hovering = tablet.hovering || (tablet.inProximity && !tablet.contact);
    event.barrelButtonDown = tablet.barrelButtonDown;
    event.eraserActive = isEraser(tablet);
    event.rotationRadians = tablet.rotationRadians;
    event.deviceState = tabletDeviceState(tablet);
    return event;
}

PointerEvent normalizeTouchGesturePointerEvent(const InputNormalizer &, const TouchGestureState &gesture)
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
