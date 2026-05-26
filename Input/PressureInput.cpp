//
// Created by Justmoong on 2026 May 26.
//

#include "PressureInput.h"

#include <algorithm>
#include <cmath>

namespace {

struct PressureCurve {
    Types::Scalar minimum = 0.0;
    Types::Scalar center = 0.5;
    Types::Scalar maximum = 1.0;
};

Types::Scalar clamp01(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Scalar finiteUnitOrDefault(Types::Scalar value, Types::Scalar fallback)
{
    if (!std::isfinite(value)) {
        return fallback;
    }
    return clamp01(value);
}

PressureCurve resolvedPressureCurve(const PressureInput &input)
{
    PressureCurve curve;
    curve.minimum = finiteUnitOrDefault(input.curveMinimum, 0.0);
    curve.maximum = finiteUnitOrDefault(input.curveMaximum, 1.0);
    if (curve.maximum < curve.minimum) {
        curve.maximum = curve.minimum;
    }
    curve.center = std::clamp(finiteUnitOrDefault(input.curveCenter, 0.5),
                              curve.minimum,
                              curve.maximum);
    return curve;
}

Types::Scalar applyPressureCurve(Types::Scalar pressure, const PressureInput &input)
{
    const Types::Scalar clampedPressure = clamp01(pressure);
    const PressureCurve curve = resolvedPressureCurve(input);
    if (clampedPressure <= 0.5) {
        return curve.minimum + (curve.center - curve.minimum) * (clampedPressure * 2.0);
    }
    return curve.center + (curve.maximum - curve.center) * ((clampedPressure - 0.5) * 2.0);
}

} // namespace

Types::Scalar resolvePressureInput(const PressureInput &input)
{
    if (!input.enabled) {
        return input.contact && !input.hovering ? 1.0 : 0.0;
    }
    if (!input.contact || input.hovering) {
        return 0.0;
    }

    const Types::Scalar range = input.maximum - input.minimum;
    if (range <= 0.0) {
        return applyPressureCurve(input.rawPressure, input);
    }
    return applyPressureCurve((input.rawPressure - input.minimum) / range, input);
}

bool pressureInputHasContact(Types::Scalar rawPressure)
{
    return rawPressure > 0.0;
}

bool pressureInputHasVariablePressure(Types::Scalar rawPressure)
{
    const Types::Scalar pressure = clamp01(rawPressure);
    return pressure > 0.0 && pressure < 1.0;
}
