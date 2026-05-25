//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeInput.h"

enum class StrokeInterpolationMode {
    Linear,
    CatmullRom,
};

struct StrokeResampler {
    StrokeInterpolationMode mode = StrokeInterpolationMode::CatmullRom;
    Types::Scalar sampleSpacing = 1.0;
};

StrokeInput resampleStrokeInput(const StrokeInput &input, const StrokeResampler &resampler);

StrokeCurve makeResampledStrokeCurve(const StrokeInput &input, const StrokeResampler &resampler);
