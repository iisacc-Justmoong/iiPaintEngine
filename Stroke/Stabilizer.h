//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"
#include "Stroke/StrokeInput.h"

struct Stabilizer {
    Types::Scalar smoothing = 0.0;
};

StrokeInput stabilizeStrokeInput(const StrokeInput &input, const Stabilizer &stabilizer);
