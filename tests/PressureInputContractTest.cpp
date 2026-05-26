#include <cmath>

#include "Input/PressureInput.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.000001;
}

} // namespace

int main()
{
    PressureInput calibrated;
    calibrated.rawPressure = 0.4;
    calibrated.minimum = 0.1;
    calibrated.maximum = 0.9;
    calibrated.contact = true;
    if (!nearlyEqual(resolvePressureInput(calibrated), 0.375)) {
        return 1;
    }

    PressureInput graphCurve = calibrated;
    graphCurve.minimum = 0.0;
    graphCurve.maximum = 1.0;
    graphCurve.curveMinimum = 0.2;
    graphCurve.curveCenter = 0.6;
    graphCurve.curveMaximum = 0.9;
    graphCurve.rawPressure = 0.0;
    if (!nearlyEqual(resolvePressureInput(graphCurve), 0.2)) {
        return 1;
    }

    graphCurve.rawPressure = 0.25;
    if (!nearlyEqual(resolvePressureInput(graphCurve), 0.4)) {
        return 1;
    }

    graphCurve.rawPressure = 0.5;
    if (!nearlyEqual(resolvePressureInput(graphCurve), 0.6)) {
        return 1;
    }

    graphCurve.rawPressure = 0.75;
    if (!nearlyEqual(resolvePressureInput(graphCurve), 0.75)) {
        return 1;
    }

    graphCurve.rawPressure = 1.0;
    if (!nearlyEqual(resolvePressureInput(graphCurve), 0.9)) {
        return 1;
    }

    PressureInput invalidCurve = graphCurve;
    invalidCurve.curveMinimum = 0.8;
    invalidCurve.curveCenter = -1.0;
    invalidCurve.curveMaximum = 0.2;
    invalidCurve.rawPressure = 0.5;
    if (!nearlyEqual(resolvePressureInput(invalidCurve), 0.8)) {
        return 1;
    }

    PressureInput hover = calibrated;
    hover.hovering = true;
    if (!nearlyEqual(resolvePressureInput(hover), 0.0)) {
        return 1;
    }

    PressureInput released = calibrated;
    released.contact = false;
    if (!nearlyEqual(resolvePressureInput(released), 0.0)) {
        return 1;
    }

    PressureInput disabled = calibrated;
    disabled.enabled = false;
    if (!nearlyEqual(resolvePressureInput(disabled), 1.0)) {
        return 1;
    }

    disabled.contact = false;
    if (!nearlyEqual(resolvePressureInput(disabled), 0.0)) {
        return 1;
    }

    PressureInput invalidRange = calibrated;
    invalidRange.minimum = 0.9;
    invalidRange.maximum = 0.1;
    invalidRange.rawPressure = 2.0;
    if (!nearlyEqual(resolvePressureInput(invalidRange), 1.0)) {
        return 1;
    }

    if (!pressureInputHasContact(0.1)
            || pressureInputHasContact(0.0)
            || !pressureInputHasVariablePressure(0.4)
            || pressureInputHasVariablePressure(0.0)
            || pressureInputHasVariablePressure(1.0)) {
        return 1;
    }

    return 0;
}
