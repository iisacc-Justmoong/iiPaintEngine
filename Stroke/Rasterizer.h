//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <vector>

#include "Core/RasterSample.h"
#include "Core/Types.h"
#include "Stroke/StrokeCurve.h"

struct Rasterizer {
    Types::Pixel radius = 2;
    std::uint32_t argb = 0xFF000000U;
};

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve, const Rasterizer &rasterizer);
