//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"

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
};

struct BrushDynamicsInput {
    Types::Scalar pressure = 1.0;
    Types::Scalar velocity = 0.0;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    Types::Scalar randomRotation = 0.0;
    Types::Scalar randomGrain = 0.0;
};

struct BrushDynamicsResult {
    Types::Scalar sizeScale = 1.0;
    Types::Scalar opacityScale = 1.0;
    Types::Scalar flowScale = 1.0;
    Types::Scalar spacingScale = 1.0;
    bool rotationFromTilt = false;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar rotationJitterRadians = 0.0;
    Types::Scalar ellipseScaleX = 1.0;
    Types::Scalar ellipseScaleY = 1.0;
    bool textureDirectionFromTilt = false;
    Types::Scalar textureDirectionRadians = 0.0;
    Types::Scalar grain = 0.0;
};

BrushDynamicsResult resolveBrushDynamics(const BrushDynamics &dynamics,
                                         const BrushDynamicsInput &input);
