#include <cmath>

#include "Input/InputNormalizer.h"
#include "Input/InputStrokeBuilder.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.000001;
}

bool hasDeviceState(std::uint32_t state, std::uint32_t flag)
{
    return (state & flag) == flag;
}

} // namespace

int main()
{
    static_assert(PointerToolKind::Pen != PointerToolKind::Eraser);
    static_assert(TouchGesturePhase::Begin != TouchGesturePhase::Update);

    InputNormalizer normalizer;
    normalizer.pressureMin = 0.1;
    normalizer.pressureMax = 0.9;
    normalizer.tabletTiltCalibration.offsetX = -0.1;
    normalizer.tabletTiltCalibration.offsetY = 0.2;
    normalizer.tabletTiltCalibration.scaleX = 2.0;
    normalizer.tabletTiltCalibration.scaleY = 0.5;

    TabletState hover;
    hover.documentPosition = {10.0, 20.0};
    hover.inProximity = true;
    hover.hovering = true;
    hover.pressure = 0.0;
    hover.tiltX = 0.3;
    hover.tiltY = 0.4;
    hover.time = 1.0;

    const PointerEvent hoverEvent = normalizeTabletPointerEvent(normalizer,
                                                               hover,
                                                               PointerEventPhase::Move);
    if (hoverEvent.device != PointerDeviceKind::Tablet
            || hoverEvent.tool != PointerToolKind::Pen
            || !hoverEvent.hovering
            || hoverEvent.primaryButtonDown
            || !nearlyEqual(hoverEvent.pressure, 0.0)
            || !nearlyEqual(hoverEvent.tiltX, 0.4)
            || !nearlyEqual(hoverEvent.tiltY, 0.3)
            || !hasDeviceState(hoverEvent.deviceState, PointerDeviceStateHover)) {
        return 1;
    }

    InputStrokeBuilder builder;
    const InputStrokeBuildResult ignoredHover = appendPointerEvent(builder, hoverEvent);
    if (ignoredHover.strokeCompleted || builder.active || !builder.points.empty()) {
        return 1;
    }

    TabletState penDown = hover;
    penDown.documentPosition = {11.0, 21.0};
    penDown.hovering = false;
    penDown.contact = true;
    penDown.primaryButtonDown = true;
    penDown.barrelButtonDown = true;
    penDown.pressure = 0.5;
    penDown.rotationRadians = 1.25;
    penDown.time = 2.0;

    const PointerEvent penPress = normalizeTabletPointerEvent(normalizer,
                                                             penDown,
                                                             PointerEventPhase::Press);
    if (penPress.button != PointerButton::Primary
            || !penPress.primaryButtonDown
            || !penPress.barrelButtonDown
            || !nearlyEqual(penPress.pressure, 0.5)
            || !nearlyEqual(penPress.rotationRadians, 1.25)
            || !hasDeviceState(penPress.deviceState, PointerDeviceStatePrimaryButton)
            || !hasDeviceState(penPress.deviceState, PointerDeviceStateBarrelButton)) {
        return 1;
    }

    appendPointerEvent(builder, penPress);
    if (!builder.active || builder.points.size() != 1) {
        return 1;
    }

    const StrokePoint &firstPoint = builder.points.front();
    if (!nearlyEqual(firstPoint.pressure, 0.5)
            || !nearlyEqual(firstPoint.tiltX, 0.4)
            || !nearlyEqual(firstPoint.tiltY, 0.3)
            || !nearlyEqual(firstPoint.rotationRadians, 1.25)
            || !hasDeviceState(firstPoint.deviceState, PointerDeviceStateBarrelButton)) {
        return 1;
    }

    TabletState eraserDown = penDown;
    eraserDown.tool = TabletToolKind::Eraser;
    eraserDown.eraser = true;
    eraserDown.barrelButtonDown = false;
    eraserDown.pressure = 0.9;
    const PointerEvent eraserEvent = normalizeTabletPointerEvent(normalizer,
                                                                eraserDown,
                                                                PointerEventPhase::Move);
    if (eraserEvent.tool != PointerToolKind::Eraser
            || !eraserEvent.eraserActive
            || eraserEvent.button != PointerButton::Eraser
            || !nearlyEqual(eraserEvent.pressure, 1.0)
            || !hasDeviceState(eraserEvent.deviceState, PointerDeviceStateEraser)) {
        return 1;
    }

    TouchGestureState pinch;
    pinch.phase = TouchGesturePhase::Update;
    pinch.centroid = {50.0, 60.0};
    pinch.translation = {5.0, -3.0};
    pinch.scale = 1.5;
    pinch.rotationRadians = 0.25;
    pinch.fingerCount = 2;
    pinch.time = 3.0;

    const PointerEvent gestureEvent = normalizeTouchGesturePointerEvent(normalizer, pinch);
    if (gestureEvent.device != PointerDeviceKind::Touch
            || gestureEvent.gesturePhase != TouchGesturePhase::Update
            || gestureEvent.touchPointCount != 2
            || !nearlyEqual(gestureEvent.gestureScale, 1.5)
            || !nearlyEqual(gestureEvent.gestureRotationRadians, 0.25)) {
        return 1;
    }

    const InputStrokeBuildResult ignoredGesture = appendPointerEvent(builder, gestureEvent);
    if (ignoredGesture.strokeCompleted) {
        return 1;
    }

    return 0;
}
