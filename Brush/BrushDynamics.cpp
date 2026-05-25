//
// Created by Justmoong on 2026 May 24.
//

#include "BrushDynamics.h"

#include <algorithm>
#include <cmath>

namespace {

Types::Scalar clamp01(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Scalar positiveScale(Types::Scalar value)
{
    return std::max<Types::Scalar>(0.01, value);
}

} // namespace

BrushDynamicsResult resolveBrushDynamics(const BrushDynamics &dynamics,
                                         const BrushDynamicsInput &input)
{
    const Types::Scalar pressure = clamp01(input.pressure);
    const Types::Scalar velocity = std::max<Types::Scalar>(0.0, input.velocity);
    const Types::Scalar tiltMagnitude = std::clamp(std::hypot(input.tiltX, input.tiltY), 0.0, 1.0);
    const bool hasTilt = tiltMagnitude > 0.0;

    BrushDynamicsResult result;
    result.sizeScale = positiveScale(1.0 + (pressure - 1.0) * dynamics.pressureToSize);
    result.flowScale = clamp01(1.0 + (pressure - 1.0) * dynamics.pressureToFlow);
    result.opacityScale = clamp01(1.0 + (pressure - 1.0) * dynamics.pressureToOpacity);
    result.spacingScale = positiveScale(1.0 + velocity * dynamics.velocityToSpacing);
    result.opacityScale *= clamp01(1.0 - velocity * dynamics.velocityToOpacity);
    result.flowScale *= clamp01(1.0 - velocity * dynamics.velocityToDryOut);
    result.rotationJitterRadians = input.randomRotation * dynamics.rotationJitter;
    result.grain = clamp01(input.randomGrain * dynamics.grainJitter);

    if (hasTilt && dynamics.tiltToRotation) {
        result.rotationFromTilt = true;
        result.rotationRadians = std::atan2(input.tiltY, input.tiltX);
    }

    if (hasTilt && dynamics.tiltToEllipse > 0.0) {
        result.ellipseScaleX = positiveScale(1.0 + tiltMagnitude * dynamics.tiltToEllipse);
        result.ellipseScaleY = positiveScale(1.0 - tiltMagnitude * dynamics.tiltToEllipse * 0.5);
    }

    if (hasTilt && dynamics.tiltToTextureDirection) {
        result.textureDirectionFromTilt = true;
        result.textureDirectionRadians = std::atan2(input.tiltY, input.tiltX);
    }

    return result;
}
