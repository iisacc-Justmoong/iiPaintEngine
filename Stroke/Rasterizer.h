//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <vector>

#include "Core/RasterSample.h"
#include "Core/Types.h"
#include "Stroke/StrokeCurve.h"

enum class RasterBlendMode {
    SourceOver,
};

struct BrushDab {
    CanvasPoint position;
    Types::Scalar scale = 1.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar alpha = 1.0;
    std::uint32_t colorArgb = 0xFF000000U;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
};

struct Rasterizer {
    Types::Pixel radius = 2;
    std::uint32_t argb = 0xFF000000U;
    Types::Pixel brushWidth = 0;
    Types::Pixel brushHeight = 0;
    std::vector<Types::Byte> brushAlpha;
    Types::Scalar spacing = 1.0;
    Types::Scalar opacity = 1.0;
    Types::Scalar flow = 1.0;
    Types::Scalar density = 1.0;
    Types::Scalar pressureScale = 0.0;
    Types::Scalar velocitySpacing = 0.0;
};

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve, const Rasterizer &rasterizer);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer);

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve, const Rasterizer &rasterizer);
