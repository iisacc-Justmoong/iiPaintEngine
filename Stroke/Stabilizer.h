//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"
#include "Stroke/StrokeInput.h"

enum class StabilizerMode {
    MovingAverage,
    Line,
};

struct Stabilizer {
    Types::Scalar smoothing = 0.0;
    StabilizerMode mode = StabilizerMode::MovingAverage;
    bool predictionEnabled = false;
    Types::Scalar predictionHorizon = 0.0;
    Types::Scalar latencyCompensation = 0.0;
    bool adaptiveResamplingEnabled = false;
    Types::Scalar adaptiveMinSpacing = 0.5;
    Types::Scalar adaptiveMaxSpacing = 4.0;
    Types::Scalar adaptiveVelocityScale = 0.0;
    bool cuspPreservationEnabled = false;
    Types::Scalar cuspAngleRadians = 1.2;
    bool previewDabsMatchCursor = false;
};

StrokeInput stabilizeStrokeInput(const StrokeInput &input, const Stabilizer &stabilizer);
