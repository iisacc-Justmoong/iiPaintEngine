//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"
#include <array>
#include <vector>

enum class BrushDynamicsEasing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

enum class BrushDynamicsCombineMode {
    Multiply,
    Add,
    Replace,
};

struct BrushCurvePoint {
    Types::Scalar input = 0.0;
    Types::Scalar output = 0.0;
};

struct BrushDynamicsResponseCurve {
    bool enabled = false;
    Types::Scalar min = 1.0;
    Types::Scalar center = 1.0;
    Types::Scalar max = 1.0;
    Types::Scalar jitter = 0.0;
    BrushDynamicsEasing easing = BrushDynamicsEasing::Linear;
    std::vector<BrushCurvePoint> points; // Strictly increasing x in [0,1]; overrides min/center/max.
};

enum class BrushDynamicsSource {
    Pressure, Velocity, Tilt, TiltX, TiltY, Direction, Rotation,
    TangentialPressure, Distance, Time, Random, StrokeRandom, Custom,
};

enum class BrushDynamicsTarget {
    Size, Flow, Opacity, Hardness, Spacing, Scatter, Rotation, Roundness,
    TextureDepth, Wetness, DryOut, BristleSpread, ScatterCount,
    Hue, Saturation, Value, ColorMix,
};

struct BrushDynamicsBinding {
    bool enabled = true;
    BrushDynamicsSource source = BrushDynamicsSource::Pressure;
    BrushDynamicsTarget target = BrushDynamicsTarget::Size;
    BrushDynamicsCombineMode combineMode = BrushDynamicsCombineMode::Multiply;
    Types::Scalar inputMinimum = 0.0;
    Types::Scalar inputMaximum = 1.0;
    BrushDynamicsResponseCurve curve{true, 0.0, 0.5, 1.0};
    unsigned int customInput = 0; // One of eight application-defined scalar channels.
};

struct BrushDynamicsPropertyResponse {
    bool enabled = false;
    Types::Scalar neutral = 1.0;
    BrushDynamicsCombineMode combineMode = BrushDynamicsCombineMode::Multiply;
    BrushDynamicsResponseCurve pressure;
    BrushDynamicsResponseCurve velocity;
    BrushDynamicsResponseCurve tilt;
    BrushDynamicsResponseCurve random;
};

struct BrushDynamics {
    bool pressureInputEnabled = true;
    bool velocityInputEnabled = true;
    bool tiltInputEnabled = true;
    bool randomInputEnabled = true;
    bool pressureToSizeEnabled = true;
    bool pressureToOpacityEnabled = true;
    bool pressureToFlowEnabled = true;
    bool velocityToSpacingEnabled = true;
    bool velocityToOpacityEnabled = true;
    bool velocityToDryOutEnabled = true;
    bool tiltToEllipseEnabled = true;
    bool rotationJitterEnabled = true;
    bool grainJitterEnabled = true;
    Types::Scalar pressureToSize = 0.0;
    Types::Scalar pressureToOpacity = 0.0;
    Types::Scalar pressureToFlow = 0.0;
    Types::Scalar velocityToSpacing = 0.0;
    Types::Scalar velocityToOpacity = 0.0;
    Types::Scalar velocityToDryOut = 0.0;
    bool tiltToRotation = false;
    Types::Scalar tiltToEllipse = 0.0;
    bool tiltToTextureDirection = false;
    Types::Scalar rotationJitter = 0.0;
    Types::Scalar grainJitter = 0.0;
    BrushDynamicsPropertyResponse sizeResponse;
    BrushDynamicsPropertyResponse flowResponse;
    BrushDynamicsPropertyResponse opacityResponse;
    BrushDynamicsPropertyResponse spacingResponse;
    BrushDynamicsPropertyResponse scatterResponse;
    BrushDynamicsPropertyResponse rotationResponse{false, 0.0, BrushDynamicsCombineMode::Add};
    BrushDynamicsPropertyResponse textureDepthResponse;
    BrushDynamicsPropertyResponse wetnessResponse;
    BrushDynamicsPropertyResponse dryOutResponse;
    BrushDynamicsPropertyResponse bristleSpreadResponse;
    std::vector<BrushDynamicsBinding> bindings; // Applied in order after legacy responses.
};

struct BrushDynamicsInput {
    Types::Scalar pressure = 1.0;
    Types::Scalar velocity = 0.0;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    Types::Scalar randomRotation = 0.0;
    Types::Scalar randomGrain = 0.0;
    Types::Scalar directionRadians = 0.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar tangentialPressure = 0.0;
    Types::Scalar distance = 0.0;
    Types::Scalar elapsedTime = 0.0;
    Types::Scalar strokeRandom = 0.0;
    std::array<Types::Scalar, 8> custom{};
};

struct BrushDynamicsResult {
    Types::Scalar sizeScale = 1.0;
    Types::Scalar opacityScale = 1.0;
    Types::Scalar flowScale = 1.0;
    Types::Scalar hardnessScale = 1.0;
    Types::Scalar spacingScale = 1.0;
    Types::Scalar scatterScale = 1.0;
    bool rotationFromTilt = false;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar rotationOffsetRadians = 0.0;
    Types::Scalar rotationJitterRadians = 0.0;
    Types::Scalar ellipseScaleX = 1.0;
    Types::Scalar ellipseScaleY = 1.0;
    bool textureDirectionFromTilt = false;
    Types::Scalar textureDirectionRadians = 0.0;
    Types::Scalar textureDepthScale = 1.0;
    Types::Scalar wetnessScale = 1.0;
    Types::Scalar dryOutScale = 1.0;
    Types::Scalar bristleSpreadScale = 1.0;
    Types::Scalar grain = 0.0;
    Types::Scalar roundnessScale = 1.0;
    Types::Scalar scatterCountScale = 1.0;
    Types::Scalar hueOffset = 0.0;
    Types::Scalar saturationScale = 1.0;
    Types::Scalar valueScale = 1.0;
    Types::Scalar colorMix = 0.0;
};

Types::Scalar evaluateBrushResponseCurve(const BrushDynamicsResponseCurve &curve, Types::Scalar input);

BrushDynamicsResult resolveBrushDynamics(const BrushDynamics &dynamics,
                                         const BrushDynamicsInput &input);
