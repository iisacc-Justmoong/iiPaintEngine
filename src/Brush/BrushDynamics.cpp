//
// Created by Justmoong on 2026 May 24.
//

#include "BrushDynamics.h"

#include <algorithm>
#include <cmath>

namespace {

Types::Scalar clamp01(Types::Scalar value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

Types::Scalar positiveScale(Types::Scalar value)
{
    return std::max<Types::Scalar>(0.01, value);
}

Types::Scalar easedValue(BrushDynamicsEasing easing, Types::Scalar value)
{
    const Types::Scalar t = clamp01(value);
    switch (easing) {
        case BrushDynamicsEasing::EaseIn:
            return t * t;
        case BrushDynamicsEasing::EaseOut:
            return 1.0 - (1.0 - t) * (1.0 - t);
        case BrushDynamicsEasing::EaseInOut:
            if (t < 0.5) {
                return 2.0 * t * t;
            }
            return 1.0 - 2.0 * (1.0 - t) * (1.0 - t);
        case BrushDynamicsEasing::Linear:
            return t;
    }
    return t;
}

Types::Scalar interpolate(Types::Scalar lhs, Types::Scalar rhs, Types::Scalar amount)
{
    return lhs + (rhs - lhs) * amount;
}

Types::Scalar resolveCurve(const BrushDynamicsResponseCurve &curve,
                           Types::Scalar input,
                           Types::Scalar randomJitter,
                           bool randomEnabled)
{
    Types::Scalar value = evaluateBrushResponseCurve(curve, input);
    if (randomEnabled && curve.jitter != 0.0) {
        value += randomJitter * curve.jitter;
    }
    return value;
}

void combineCurve(Types::Scalar &value,
                  bool &used,
                  const BrushDynamicsPropertyResponse &response,
                  const BrushDynamicsResponseCurve &curve,
                  Types::Scalar input,
                  Types::Scalar randomJitter,
                  bool randomEnabled)
{
    if (!curve.enabled) {
        return;
    }

    const Types::Scalar curveValue = resolveCurve(curve, input, randomJitter, randomEnabled);
    if (!used) {
        value = response.combineMode == BrushDynamicsCombineMode::Add
                ? response.neutral + curveValue
                : curveValue;
        used = true;
        return;
    }

    if (response.combineMode == BrushDynamicsCombineMode::Replace) {
        value = curveValue;
    } else if (response.combineMode == BrushDynamicsCombineMode::Add) {
        value += curveValue;
    } else {
        value *= curveValue;
    }
}

Types::Scalar resolvePropertyResponse(const BrushDynamicsPropertyResponse &response,
                                      Types::Scalar pressure,
                                      Types::Scalar velocity,
                                      Types::Scalar tilt,
                                      Types::Scalar random,
                                      Types::Scalar randomJitter,
                                      bool pressureEnabled,
                                      bool velocityEnabled,
                                      bool tiltEnabled,
                                      bool randomEnabled)
{
    if (!response.enabled) {
        return response.neutral;
    }

    Types::Scalar value = response.neutral;
    bool used = false;
    if (pressureEnabled) {
        combineCurve(value, used, response, response.pressure, pressure, randomJitter, randomEnabled);
    }
    if (velocityEnabled) {
        combineCurve(value, used, response, response.velocity, velocity, randomJitter, randomEnabled);
    }
    if (tiltEnabled) {
        combineCurve(value, used, response, response.tilt, tilt, randomJitter, randomEnabled);
    }
    if (randomEnabled) {
        combineCurve(value, used, response, response.random, random, randomJitter, randomEnabled);
    }
    return used ? value : response.neutral;
}

} // namespace

Types::Scalar evaluateBrushResponseCurve(const BrushDynamicsResponseCurve &curve, Types::Scalar input)
{
    const auto t = clamp01(input);
    if (!curve.points.empty()) {
        if (t <= curve.points.front().input) return curve.points.front().output;
        for (std::size_t i = 1; i < curve.points.size(); ++i) {
            const auto &a = curve.points[i - 1];
            const auto &b = curve.points[i];
            if (t <= b.input && b.input > a.input)
                return interpolate(a.output, b.output, easedValue(curve.easing, (t - a.input) / (b.input - a.input)));
        }
        return curve.points.back().output;
    }
    return t < 0.5
            ? interpolate(curve.min, curve.center, easedValue(curve.easing, t * 2.0))
            : interpolate(curve.center, curve.max, easedValue(curve.easing, (t - 0.5) * 2.0));
}

BrushDynamicsResult resolveBrushDynamics(const BrushDynamics &dynamics,
                                         const BrushDynamicsInput &input)
{
    const Types::Scalar pressure = dynamics.pressureInputEnabled ? clamp01(input.pressure) : 1.0;
    const Types::Scalar velocity = dynamics.velocityInputEnabled ? std::max<Types::Scalar>(0.0, input.velocity) : 0.0;
    const Types::Scalar responseVelocity = clamp01(velocity);
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
    result.sizeScale *= positiveScale(resolvePropertyResponse(dynamics.sizeResponse,
                                                              pressure,
                                                              responseVelocity,
                                                              tiltMagnitude,
                                                              input.randomGrain,
                                                              input.randomRotation,
                                                              dynamics.pressureInputEnabled,
                                                              dynamics.velocityInputEnabled,
                                                              dynamics.tiltInputEnabled,
                                                              dynamics.randomInputEnabled));
    result.flowScale = dynamics.pressureToFlowEnabled
            ? clamp01(1.0 + (pressure - 1.0) * dynamics.pressureToFlow)
            : 1.0;
    result.flowScale *= std::max<Types::Scalar>(
            0.0,
            resolvePropertyResponse(dynamics.flowResponse,
                                    pressure,
                                    responseVelocity,
                                    tiltMagnitude,
                                    input.randomGrain,
                                    input.randomRotation,
                                    dynamics.pressureInputEnabled,
                                    dynamics.velocityInputEnabled,
                                    dynamics.tiltInputEnabled,
                                    dynamics.randomInputEnabled));
    result.opacityScale = dynamics.pressureToOpacityEnabled
            ? clamp01(1.0 + (pressure - 1.0) * dynamics.pressureToOpacity)
            : 1.0;
    result.opacityScale *= std::max<Types::Scalar>(
            0.0,
            resolvePropertyResponse(dynamics.opacityResponse,
                                    pressure,
                                    responseVelocity,
                                    tiltMagnitude,
                                    input.randomGrain,
                                    input.randomRotation,
                                    dynamics.pressureInputEnabled,
                                    dynamics.velocityInputEnabled,
                                    dynamics.tiltInputEnabled,
                                    dynamics.randomInputEnabled));
    result.spacingScale = dynamics.velocityToSpacingEnabled
            ? positiveScale(1.0 + velocity * dynamics.velocityToSpacing)
            : 1.0;
    result.spacingScale *= positiveScale(resolvePropertyResponse(dynamics.spacingResponse,
                                                                 pressure,
                                                                 responseVelocity,
                                                                 tiltMagnitude,
                                                                 input.randomGrain,
                                                                 input.randomRotation,
                                                                 dynamics.pressureInputEnabled,
                                                                 dynamics.velocityInputEnabled,
                                                                 dynamics.tiltInputEnabled,
                                                                 dynamics.randomInputEnabled));
    if (dynamics.velocityToOpacityEnabled) {
        result.opacityScale *= clamp01(1.0 - velocity * dynamics.velocityToOpacity);
    }
    if (dynamics.velocityToDryOutEnabled) {
        result.flowScale *= clamp01(1.0 - velocity * dynamics.velocityToDryOut);
    }
    result.rotationJitterRadians = randomRotation * dynamics.rotationJitter;
    result.grain = clamp01(randomGrain * dynamics.grainJitter);
    result.scatterScale = std::max<Types::Scalar>(
            0.0,
            resolvePropertyResponse(dynamics.scatterResponse,
                                    pressure,
                                    responseVelocity,
                                    tiltMagnitude,
                                    input.randomGrain,
                                    input.randomRotation,
                                    dynamics.pressureInputEnabled,
                                    dynamics.velocityInputEnabled,
                                    dynamics.tiltInputEnabled,
                                    dynamics.randomInputEnabled));
    result.rotationOffsetRadians = resolvePropertyResponse(dynamics.rotationResponse,
                                                           pressure,
                                                           responseVelocity,
                                                           tiltMagnitude,
                                                           input.randomGrain,
                                                           input.randomRotation,
                                                           dynamics.pressureInputEnabled,
                                                           dynamics.velocityInputEnabled,
                                                           dynamics.tiltInputEnabled,
                                                           dynamics.randomInputEnabled);
    result.textureDepthScale = std::max<Types::Scalar>(
            0.0,
            resolvePropertyResponse(dynamics.textureDepthResponse,
                                    pressure,
                                    responseVelocity,
                                    tiltMagnitude,
                                    input.randomGrain,
                                    input.randomRotation,
                                    dynamics.pressureInputEnabled,
                                    dynamics.velocityInputEnabled,
                                    dynamics.tiltInputEnabled,
                                    dynamics.randomInputEnabled));
    result.wetnessScale = std::max<Types::Scalar>(
            0.0,
            resolvePropertyResponse(dynamics.wetnessResponse,
                                    pressure,
                                    responseVelocity,
                                    tiltMagnitude,
                                    input.randomGrain,
                                    input.randomRotation,
                                    dynamics.pressureInputEnabled,
                                    dynamics.velocityInputEnabled,
                                    dynamics.tiltInputEnabled,
                                    dynamics.randomInputEnabled));
    result.dryOutScale = clamp01(resolvePropertyResponse(dynamics.dryOutResponse,
                                                         pressure,
                                                         responseVelocity,
                                                         tiltMagnitude,
                                                         input.randomGrain,
                                                         input.randomRotation,
                                                         dynamics.pressureInputEnabled,
                                                         dynamics.velocityInputEnabled,
                                                         dynamics.tiltInputEnabled,
                                                         dynamics.randomInputEnabled));
    result.bristleSpreadScale = std::max<Types::Scalar>(
            0.0,
            resolvePropertyResponse(dynamics.bristleSpreadResponse,
                                    pressure,
                                    responseVelocity,
                                    tiltMagnitude,
                                    input.randomGrain,
                                    input.randomRotation,
                                    dynamics.pressureInputEnabled,
                                    dynamics.velocityInputEnabled,
                                    dynamics.tiltInputEnabled,
                                    dynamics.randomInputEnabled));

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

    for (const auto &binding : dynamics.bindings) {
        if (!binding.enabled || !binding.curve.enabled) continue;
        Types::Scalar source = 0.0;
        switch (binding.source) {
            case BrushDynamicsSource::Pressure:
                if (!dynamics.pressureInputEnabled) continue;
                source = pressure; break;
            case BrushDynamicsSource::Velocity:
                if (!dynamics.velocityInputEnabled) continue;
                source = velocity; break;
            case BrushDynamicsSource::Tilt:
                if (!dynamics.tiltInputEnabled) continue;
                source = tiltMagnitude; break;
            case BrushDynamicsSource::TiltX:
                if (!dynamics.tiltInputEnabled) continue;
                source = tiltX; break;
            case BrushDynamicsSource::TiltY:
                if (!dynamics.tiltInputEnabled) continue;
                source = tiltY; break;
            case BrushDynamicsSource::Direction: source = input.directionRadians; break;
            case BrushDynamicsSource::Rotation: source = input.rotationRadians; break;
            case BrushDynamicsSource::TangentialPressure: source = input.tangentialPressure; break;
            case BrushDynamicsSource::Distance: source = input.distance; break;
            case BrushDynamicsSource::Time: source = input.elapsedTime; break;
            case BrushDynamicsSource::Random:
                if (!dynamics.randomInputEnabled) continue;
                source = input.randomGrain; break;
            case BrushDynamicsSource::StrokeRandom:
                if (!dynamics.randomInputEnabled) continue;
                source = input.strokeRandom; break;
            case BrushDynamicsSource::Custom:
                if (binding.customInput >= input.custom.size()) continue;
                source = input.custom[binding.customInput]; break;
        }
        const auto span = binding.inputMaximum - binding.inputMinimum;
        if (!std::isfinite(source) || !std::isfinite(span) || span <= 0.0) continue;
        const auto value = resolveCurve(binding.curve, (source - binding.inputMinimum) / span,
                                        input.randomRotation, dynamics.randomInputEnabled);
        if (!std::isfinite(value)) continue;
        Types::Scalar *target = nullptr;
        switch (binding.target) {
            case BrushDynamicsTarget::Size: target = &result.sizeScale; break;
            case BrushDynamicsTarget::Flow: target = &result.flowScale; break;
            case BrushDynamicsTarget::Opacity: target = &result.opacityScale; break;
            case BrushDynamicsTarget::Hardness: target = &result.hardnessScale; break;
            case BrushDynamicsTarget::Spacing: target = &result.spacingScale; break;
            case BrushDynamicsTarget::Scatter: target = &result.scatterScale; break;
            case BrushDynamicsTarget::Rotation: target = &result.rotationOffsetRadians; break;
            case BrushDynamicsTarget::Roundness: target = &result.roundnessScale; break;
            case BrushDynamicsTarget::TextureDepth: target = &result.textureDepthScale; break;
            case BrushDynamicsTarget::Wetness: target = &result.wetnessScale; break;
            case BrushDynamicsTarget::DryOut: target = &result.dryOutScale; break;
            case BrushDynamicsTarget::BristleSpread: target = &result.bristleSpreadScale; break;
            case BrushDynamicsTarget::ScatterCount: target = &result.scatterCountScale; break;
            case BrushDynamicsTarget::Hue: target = &result.hueOffset; break;
            case BrushDynamicsTarget::Saturation: target = &result.saturationScale; break;
            case BrushDynamicsTarget::Value: target = &result.valueScale; break;
            case BrushDynamicsTarget::ColorMix: target = &result.colorMix; break;
        }
        if (!target) continue;
        switch (binding.combineMode) {
            case BrushDynamicsCombineMode::Multiply: *target *= value; break;
            case BrushDynamicsCombineMode::Add: *target += value; break;
            case BrushDynamicsCombineMode::Replace: *target = value; break;
        }
        const bool signedTarget = binding.target == BrushDynamicsTarget::Rotation || binding.target == BrushDynamicsTarget::Hue;
        *target = std::clamp(*target, signedTarget ? -1024.0 : 0.0, 1024.0);
    }
    return result;
}
