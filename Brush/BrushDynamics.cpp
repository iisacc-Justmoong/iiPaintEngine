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
    const Types::Scalar pressure = dynamics.pressureInputEnabled ? clamp01(input.pressure) : 1.0;
    const Types::Scalar velocity = dynamics.velocityInputEnabled ? std::max<Types::Scalar>(0.0, input.velocity) : 0.0;
    const Types::Scalar tiltX = dynamics.tiltInputEnabled ? input.tiltX : 0.0;
    const Types::Scalar tiltY = dynamics.tiltInputEnabled ? input.tiltY : 0.0;
    const Types::Scalar tiltMagnitude = std::clamp(std::hypot(tiltX, tiltY), 0.0, 1.0);
    const bool hasTilt = tiltMagnitude > 0.0;
    const Types::Scalar randomRotation = dynamics.randomInputEnabled && dynamics.rotationJitterEnabled
            ? input.randomRotation
            : 0.0;
    const Types::Scalar randomGrain = dynamics.randomInputEnabled && dynamics.grainJitterEnabled
            ? input.randomGrain
            : 0.0;

    BrushDynamicsResult result;
    result.sizeScale = dynamics.pressureToSizeEnabled
            ? positiveScale(1.0 + (pressure - 1.0) * dynamics.pressureToSize)
            : 1.0;
    result.flowScale = dynamics.pressureToFlowEnabled
            ? clamp01(1.0 + (pressure - 1.0) * dynamics.pressureToFlow)
            : 1.0;
    result.opacityScale = dynamics.pressureToOpacityEnabled
            ? clamp01(1.0 + (pressure - 1.0) * dynamics.pressureToOpacity)
            : 1.0;
    result.spacingScale = dynamics.velocityToSpacingEnabled
            ? positiveScale(1.0 + velocity * dynamics.velocityToSpacing)
            : 1.0;
    if (dynamics.velocityToOpacityEnabled) {
        result.opacityScale *= clamp01(1.0 - velocity * dynamics.velocityToOpacity);
    }
    if (dynamics.velocityToDryOutEnabled) {
        result.flowScale *= clamp01(1.0 - velocity * dynamics.velocityToDryOut);
    }
    result.rotationJitterRadians = randomRotation * dynamics.rotationJitter;
    result.grain = clamp01(randomGrain * dynamics.grainJitter);

    if (hasTilt && dynamics.tiltToRotation) {
        result.rotationFromTilt = true;
        result.rotationRadians = std::atan2(tiltY, tiltX);
    }

    if (hasTilt && dynamics.tiltToEllipseEnabled && dynamics.tiltToEllipse > 0.0) {
        result.ellipseScaleX = positiveScale(1.0 + tiltMagnitude * dynamics.tiltToEllipse);
        result.ellipseScaleY = positiveScale(1.0 - tiltMagnitude * dynamics.tiltToEllipse * 0.5);
    }

    if (hasTilt && dynamics.tiltToTextureDirection) {
        result.textureDirectionFromTilt = true;
        result.textureDirectionRadians = std::atan2(tiltY, tiltX);
    }

    return result;
}
