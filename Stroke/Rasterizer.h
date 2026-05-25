//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <vector>

#include "Core/PaintRect.h"
#include "Core/RasterSample.h"
#include "Core/Types.h"
#include "Stroke/StrokeCurve.h"

struct BrushDynamics;

struct BrushDab {
    DocumentPoint position;
    Types::Scalar scale = 1.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar alpha = 1.0;
    Types::Scalar opacityCapScale = 1.0;
    Types::Scalar ellipseScaleX = 1.0;
    Types::Scalar ellipseScaleY = 1.0;
    Types::Scalar textureDirectionRadians = 0.0;
    Types::Scalar grain = 0.0;
    std::uint32_t colorArgb = 0xFF000000U;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
    std::uint32_t sequenceIndex = 0;
};

using DabCommand = BrushDab;

struct RasterProjection {
    DocumentPoint documentOrigin{};
    DevicePixelPoint deviceOrigin{};
    Types::Scalar scale = 1.0;
};

struct Rasterizer {
    Types::Pixel radius = 2;
    std::uint32_t argb = 0xFF000000U;
    Types::Scalar brushSize = 0.0;
    Types::Pixel brushWidth = 0;
    Types::Pixel brushHeight = 0;
    std::vector<Types::Byte> brushAlpha;
    Types::Scalar spacing = 1.0;
    Types::Scalar spacingRatio = 1.0;
    Types::Scalar opacity = 1.0;
    Types::Scalar flow = 1.0;
    Types::Scalar hardness = 1.0;
    Types::Scalar density = 1.0;
    Types::Scalar pressureScale = 0.0;
    Types::Scalar velocitySpacing = 0.0;
    Types::Scalar warmupDistance = 0.0;
    Types::Scalar taperDistance = 0.0;
    Types::Scalar rotationJitter = 0.0;
};

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve, const Rasterizer &rasterizer);

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     std::uint32_t randomSeed);

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     const BrushDynamics &dynamics,
                                     std::uint32_t randomSeed);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection);

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve, const Rasterizer &rasterizer);

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve,
                                               const Rasterizer &rasterizer,
                                               const RasterProjection &projection);

DocumentRect documentBoundsForBrushDab(const BrushDab &dab, const Rasterizer &rasterizer);

DocumentRect documentBoundsForBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer);

std::vector<DocumentRect> documentBoundsForEachBrushDab(const std::vector<BrushDab> &dabs,
                                                       const Rasterizer &rasterizer);

DevicePixelRect deviceBoundsForBrushDab(const BrushDab &dab,
                                        const Rasterizer &rasterizer,
                                        const RasterProjection &projection);

std::vector<DevicePixelRect> deviceBoundsForBrushDabs(const std::vector<BrushDab> &dabs,
                                                      const Rasterizer &rasterizer,
                                                      const RasterProjection &projection);
