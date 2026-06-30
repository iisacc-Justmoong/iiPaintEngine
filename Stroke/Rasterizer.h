//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "Core/PaintRect.h"
#include "Core/RasterSample.h"
#include "Core/Types.h"
#include "Stroke/StrokeCurve.h"

struct BrushDynamics;
struct BrushMaterial;

enum class StrokeTaperShape {
    Linear,
    EaseIn,
    EaseOut,
    SmoothStep,
};

struct BrushDab {
    DocumentPoint position;
    Types::Scalar scale = 1.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar alpha = 1.0;
    Types::Scalar opacityCapScale = 1.0;
    Types::Scalar hardnessScale = 1.0;
    Types::Scalar ellipseScaleX = 1.0;
    Types::Scalar ellipseScaleY = 1.0;
    Types::Scalar textureDirectionRadians = 0.0;
    Types::Scalar grain = 0.0;
    Types::Scalar textureAlpha = 1.0;
    Types::Scalar textureDepthScale = 1.0;
    Types::Scalar textureScale = 1.0;
    Types::Scalar textureRotationRadians = 0.0;
    Types::Scalar wetnessScale = 1.0;
    Types::Scalar dryOutScale = 1.0;
    Types::Scalar bristleSpreadScale = 1.0;
    Types::Scalar scatterScale = 1.0;
    Types::Scalar dualBrushScale = 1.0;
    Types::Scalar dualBrushRotationRadians = 0.0;
    Types::Scalar strokeDistance = 0.0;
    bool dualBrush = false;
    std::uint32_t colorArgb = 0xFF000000U;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
    std::uint32_t sequenceIndex = 0;
};

using DabCommand = BrushDab;

struct BrushDabSpan {
    const BrushDab *values = nullptr;
    std::size_t valueCount = 0;

    constexpr BrushDabSpan() = default;

    constexpr BrushDabSpan(const BrushDab *data, std::size_t size)
        : values(data)
        , valueCount(size)
    {
    }

    explicit BrushDabSpan(const std::vector<BrushDab> &dabs)
        : values(dabs.data())
        , valueCount(dabs.size())
    {
    }

    [[nodiscard]] constexpr const BrushDab *data() const
    {
        return values;
    }

    [[nodiscard]] constexpr std::size_t size() const
    {
        return valueCount;
    }

    [[nodiscard]] constexpr bool empty() const
    {
        return valueCount == 0;
    }

    [[nodiscard]] constexpr const BrushDab *begin() const
    {
        return values;
    }

    [[nodiscard]] constexpr const BrushDab *end() const
    {
        return values == nullptr ? nullptr : values + valueCount;
    }
};

struct RasterProjection {
    DocumentPoint documentOrigin{};
    DevicePixelPoint deviceOrigin{};
    Types::Scalar scale = 1.0;
};

struct RasterSourceSampler {
    using SampleArgb = std::uint32_t (*)(const void *context, DevicePixelPoint position);

    const void *context = nullptr;
    SampleArgb sampleArgb = nullptr;
    DevicePixelPoint origin{};
    Types::Pixel width = 0;
    Types::Pixel height = 0;
};

struct Rasterizer {
    Types::Pixel radius = 2;
    std::uint32_t argb = 0xFF000000U;
    Types::Scalar brushSize = 0.0;
    Types::Pixel brushWidth = 0;
    Types::Pixel brushHeight = 0;
    std::vector<Types::Byte> brushAlpha;
    Types::Scalar spacing = 0.0;
    Types::Scalar spacingRatio = 0.0;
    bool spacingEnabled = true;
    Types::Scalar opacity = 1.0;
    bool opacityEnabled = true;
    Types::Scalar flow = 1.0;
    bool flowEnabled = true;
    Types::Scalar hardness = 1.0;
    bool hardnessEnabled = true;
    Types::Scalar density = 1.0;
    Types::Scalar pressureScale = 0.0;
    Types::Scalar velocitySpacing = 0.0;
    Types::Scalar warmupDistance = 0.0;
    Types::Scalar taperDistance = 0.0;
    Types::Scalar taperMinimum = 0.25;
    StrokeTaperShape warmupTaperShape = StrokeTaperShape::Linear;
    StrokeTaperShape endTaperShape = StrokeTaperShape::Linear;
    Types::Scalar rotationJitter = 0.0;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
};

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve, const Rasterizer &rasterizer);

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     std::uint32_t randomSeed);

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     const BrushDynamics &dynamics,
                                     std::uint32_t randomSeed);

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     const BrushDynamics &dynamics,
                                     const BrushMaterial &material,
                                     std::uint32_t randomSeed);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer);

std::vector<RasterSample> projectBrushDabs(BrushDabSpan dabs, const Rasterizer &rasterizer);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection);

std::vector<RasterSample> projectBrushDabs(BrushDabSpan dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const BrushMaterial &material);

std::vector<RasterSample> projectBrushDabs(BrushDabSpan dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const BrushMaterial &material);

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const RasterSourceSampler &sourceSampler,
                                           const BrushMaterial &material);

std::vector<RasterSample> projectBrushDabs(BrushDabSpan dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const RasterSourceSampler &sourceSampler,
                                           const BrushMaterial &material);

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

DevicePixelRect deviceBoundsForBrushDabsUnion(const std::vector<BrushDab> &dabs,
                                              const Rasterizer &rasterizer,
                                              const RasterProjection &projection);

DevicePixelRect deviceBoundsForBrushDabsUnion(BrushDabSpan dabs,
                                              const Rasterizer &rasterizer,
                                              const RasterProjection &projection);

std::vector<DevicePixelRect> deviceBoundsForBrushDabs(const std::vector<BrushDab> &dabs,
                                                      const Rasterizer &rasterizer,
                                                      const RasterProjection &projection);

std::vector<DevicePixelRect> deviceBoundsForBrushDabs(BrushDabSpan dabs,
                                                      const Rasterizer &rasterizer,
                                                      const RasterProjection &projection);
