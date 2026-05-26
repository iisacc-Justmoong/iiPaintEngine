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

    TabletState penRelease = penDown;
    penRelease.contact = false;
    penRelease.primaryButtonDown = false;
    penRelease.barrelButtonDown = false;
    penRelease.pressure = 0.0;
    penRelease.time = 2.5;
    const PointerEvent releaseEvent = normalizeTabletPointerEvent(normalizer,
                                                                  penRelease,
                                                                  PointerEventPhase::Release);
    const InputStrokeBuildResult completedPenStroke = appendPointerEvent(builder, releaseEvent);
    if (releaseEvent.hovering
            || hasDeviceState(releaseEvent.deviceState, PointerDeviceStateHover)
            || !completedPenStroke.strokeCompleted
            || builder.active) {
        return 1;
    }

    InputStrokeBuilder lateContactBuilder;
    TabletState lateContactMove = penDown;
    lateContactMove.documentPosition = {12.0, 22.0};
    lateContactMove.primaryButtonDown = false;
    lateContactMove.barrelButtonDown = false;
    lateContactMove.pressure = 0.4;
    lateContactMove.time = 3.0;
    const PointerEvent lateContactMoveEvent = normalizeTabletPointerEvent(normalizer,
                                                                         lateContactMove,
                                                                         PointerEventPhase::Move);
    const InputStrokeBuildResult lateContactMoveResult = appendPointerEvent(lateContactBuilder,
                                                                           lateContactMoveEvent);
    if (lateContactMoveResult.strokeCompleted
            || !lateContactBuilder.active
            || lateContactBuilder.points.size() != 1
            || !nearlyEqual(lateContactBuilder.points.front().pressure, 0.375)) {
        return 1;
    }

    InputNormalizer graphNormalizer = normalizer;
    graphNormalizer.pressureMin = 0.0;
    graphNormalizer.pressureMax = 1.0;
    graphNormalizer.pressureCurveMinimum = 0.2;
    graphNormalizer.pressureCurveCenter = 0.6;
    graphNormalizer.pressureCurveMaximum = 0.9;
    TabletState graphPen = penDown;
    graphPen.pressure = 0.25;
    const PointerEvent graphPenEvent = normalizeTabletPointerEvent(graphNormalizer,
                                                                   graphPen,
                                                                   PointerEventPhase::Move);
    if (!nearlyEqual(graphPenEvent.pressure, 0.4)) {
        return 1;
    }

    TabletState lateContactRelease = lateContactMove;
    lateContactRelease.contact = false;
    lateContactRelease.primaryButtonDown = false;
    lateContactRelease.pressure = 0.0;
    lateContactRelease.time = 3.2;
    const PointerEvent lateContactReleaseEvent = normalizeTabletPointerEvent(normalizer,
                                                                            lateContactRelease,
                                                                            PointerEventPhase::Release);
    const InputStrokeBuildResult lateContactCompleted = appendPointerEvent(lateContactBuilder,
                                                                          lateContactReleaseEvent);
    if (!lateContactCompleted.strokeCompleted
            || lateContactBuilder.active
            || lateContactCompleted.stroke.points.size() != 2) {
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

    InputNormalizer disabledNormalizer = normalizer;
    disabledNormalizer.pressureEnabled = false;
    disabledNormalizer.tiltEnabled = false;
    disabledNormalizer.rotationEnabled = false;
    disabledNormalizer.hoverEnabled = false;
    disabledNormalizer.barrelButtonEnabled = false;
    disabledNormalizer.eraserEnabled = false;
    disabledNormalizer.touchGestureEnabled = false;

    const PointerEvent disabledPen = normalizeTabletPointerEvent(disabledNormalizer,
                                                                 eraserDown,
                                                                 PointerEventPhase::Press);
    if (!nearlyEqual(disabledPen.pressure, 1.0)
            || !nearlyEqual(disabledPen.tiltX, 0.0)
            || !nearlyEqual(disabledPen.tiltY, 0.0)
            || !nearlyEqual(disabledPen.rotationRadians, 0.0)
            || disabledPen.barrelButtonDown
            || disabledPen.eraserActive
            || disabledPen.tool != PointerToolKind::Pen
            || disabledPen.button != PointerButton::Primary
            || hasDeviceState(disabledPen.deviceState, PointerDeviceStateBarrelButton)
            || hasDeviceState(disabledPen.deviceState, PointerDeviceStateEraser)) {
        return 1;
    }

    const PointerEvent disabledHover = normalizeTabletPointerEvent(disabledNormalizer,
                                                                   hover,
                                                                   PointerEventPhase::Move);
    if (disabledHover.hovering
            || hasDeviceState(disabledHover.deviceState, PointerDeviceStateHover)) {
        return 1;
    }

    const PointerEvent disabledGesture = normalizeTouchGesturePointerEvent(disabledNormalizer, pinch);
    if (disabledGesture.gesturePhase != TouchGesturePhase::None
            || disabledGesture.touchPointCount != 0
            || !nearlyEqual(disabledGesture.gestureScale, 1.0)
            || !nearlyEqual(disabledGesture.gestureRotationRadians, 0.0)
            || hasDeviceState(disabledGesture.deviceState, PointerDeviceStateTouchGesture)) {
        return 1;
    }

    return 0;
}
