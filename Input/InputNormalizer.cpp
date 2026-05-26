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
    if (!normalizer.pressureEnabled) {
        return tablet.contact && !tablet.hovering ? 1.0 : 0.0;
    }
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

bool isHovering(const InputNormalizer &normalizer, const TabletState &tablet)
{
    return normalizer.hoverEnabled && (tablet.hovering || (tablet.inProximity && !tablet.contact));
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

std::uint32_t tabletDeviceState(const InputNormalizer &normalizer, const TabletState &tablet)
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
    if (isHovering(normalizer, tablet)) {
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
    event.button = tabletButton(normalizer, tablet);
    event.hovering = isHovering(normalizer, tablet);
    event.primaryButtonDown = tablet.contact && !event.hovering;
    event.tiltX = calibratedTiltX(normalizer, tablet);
    event.tiltY = calibratedTiltY(normalizer, tablet);
    event.tool = isEraser(normalizer, tablet) ? PointerToolKind::Eraser : PointerToolKind::Pen;
    event.barrelButtonDown = normalizer.barrelButtonEnabled && tablet.barrelButtonDown;
    event.eraserActive = isEraser(normalizer, tablet);
    event.rotationRadians = normalizer.rotationEnabled ? tablet.rotationRadians : 0.0;
    event.deviceState = tabletDeviceState(normalizer, tablet);
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
