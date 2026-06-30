//
// Created by Justmoong on 2026 May 24.
//

#include "Rasterizer.h"

#include "Brush/BrushDynamics.h"
#include "Brush/BrushMaterial.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace {

struct UnitColor {
    Types::Scalar red = 0.0;
    Types::Scalar green = 0.0;
    Types::Scalar blue = 0.0;
    Types::Scalar alpha = 0.0;
};

struct ProjectionContext {
    const RasterSourceSampler *sourceSampler = nullptr;
    const BrushMaterial *material = nullptr;
    RasterProjection projection{};
};

std::size_t projectedSampleCapacity(std::span<const BrushDab> dabs, const Rasterizer &rasterizer)
{
    if (dabs.empty()) {
        return 0;
    }

    const Types::Pixel footprintWidth = rasterizer.brushWidth > 0
            ? rasterizer.brushWidth
            : std::max<Types::Pixel>(1, rasterizer.radius * 2 + 1);
    const Types::Pixel footprintHeight = rasterizer.brushHeight > 0
            ? rasterizer.brushHeight
            : std::max<Types::Pixel>(1, rasterizer.radius * 2 + 1);
    return dabs.size()
            * static_cast<std::size_t>(footprintWidth)
            * static_cast<std::size_t>(footprintHeight);
}

Types::Scalar materialProjectionMask(Types::Scalar maskAlpha,
                                     const BrushDab &dab,
                                     DevicePixelPoint position,
                                     const ProjectionContext &context);

Types::Scalar clamp01(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

std::uint8_t alphaByte(Types::Scalar value)
{
    return static_cast<std::uint8_t>(std::clamp<std::uint32_t>(
            static_cast<std::uint32_t>(std::lround(value)),
            0U,
            255U));
}

std::uint8_t sourceAlpha(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

UnitColor unitColorFromArgb(std::uint32_t argb)
{
    return UnitColor{
            static_cast<Types::Scalar>((argb >> 16U) & 0xFFU) / 255.0,
            static_cast<Types::Scalar>((argb >> 8U) & 0xFFU) / 255.0,
            static_cast<Types::Scalar>(argb & 0xFFU) / 255.0,
            static_cast<Types::Scalar>(sourceAlpha(argb)) / 255.0,
    };
}

UnitColor mixColor(UnitColor lhs, UnitColor rhs, Types::Scalar amount)
{
    const Types::Scalar t = clamp01(amount);
    return UnitColor{
            lhs.red * (1.0 - t) + rhs.red * t,
            lhs.green * (1.0 - t) + rhs.green * t,
            lhs.blue * (1.0 - t) + rhs.blue * t,
            lhs.alpha * (1.0 - t) + rhs.alpha * t,
    };
}

std::uint8_t byteFromUnit(Types::Scalar value)
{
    return alphaByte(clamp01(value) * 255.0);
}

std::uint32_t argbFromUnitColor(UnitColor color)
{
    return (static_cast<std::uint32_t>(byteFromUnit(color.alpha)) << 24U)
            | (static_cast<std::uint32_t>(byteFromUnit(color.red)) << 16U)
            | (static_cast<std::uint32_t>(byteFromUnit(color.green)) << 8U)
            | static_cast<std::uint32_t>(byteFromUnit(color.blue));
}

std::uint32_t withAlpha(std::uint32_t argb, std::uint8_t alpha)
{
    return (argb & 0x00FFFFFFU) | ((alpha & 0xFFU) << 24U);
}

bool hasSourceLayer(const ProjectionContext &context)
{
    return context.sourceSampler != nullptr
            && context.sourceSampler->sampleArgb != nullptr
            && context.sourceSampler->width > 0
            && context.sourceSampler->height > 0;
}

std::uint32_t sampledSourceArgb(const RasterSourceSampler &sourceSampler, DevicePixelPoint position)
{
    const DevicePixelPoint localPosition{
            position.x - sourceSampler.origin.x,
            position.y - sourceSampler.origin.y,
    };
    if (localPosition.x < 0
            || localPosition.y < 0
            || localPosition.x >= sourceSampler.width
            || localPosition.y >= sourceSampler.height) {
        return 0x00000000U;
    }

    return sourceSampler.sampleArgb(sourceSampler.context, localPosition);
}

std::uint32_t wetDabColorArgb(const BrushDab &dab,
                              const Rasterizer &rasterizer,
                              const ProjectionContext &context,
                              DevicePixelPoint position)
{
    if (!hasSourceLayer(context)
            || context.material == nullptr
            || !context.material->simulation.enabled
            || context.material->simulation.model == BrushSimulationModel::Dry) {
        return dab.colorArgb;
    }

    const BrushSimulation &simulation = context.material->simulation;
    const UnitColor brush = unitColorFromArgb(dab.colorArgb);
    const Types::Scalar wetness = clamp01(simulation.wetness * dab.wetnessScale * dab.dryOutScale);
    const Types::Scalar smudge = clamp01(simulation.smudgeStrength * wetness);
    const Types::Scalar pickup = clamp01(simulation.pickup);
    const Types::Scalar deposit = clamp01(simulation.deposit);
    const Types::Scalar mix = clamp01(simulation.mixStrength * std::max<Types::Scalar>(wetness, 0.01));
    const Types::Scalar baseSize = rasterizer.brushSize > 0.0
            ? rasterizer.brushSize
            : static_cast<Types::Scalar>(std::max<Types::Pixel>(1, rasterizer.radius * 2));
    const Types::Scalar bristleDrag = context.material->bristle.enabled
            ? std::max<Types::Scalar>(0.0, context.material->bristle.length * dab.bristleSpreadScale)
                    * (1.0 - clamp01(context.material->bristle.stiffness)) * 0.1
            : 0.0;
    const Types::Scalar smudgeDistance = std::max<Types::Scalar>(1.0, baseSize * wetness * 0.5 + bristleDrag);
    const Types::Pixel pulledX = static_cast<Types::Pixel>(
            std::lround(static_cast<Types::Scalar>(position.x) - std::cos(dab.rotationRadians) * smudgeDistance));
    const Types::Pixel pulledY = static_cast<Types::Pixel>(
            std::lround(static_cast<Types::Scalar>(position.y) - std::sin(dab.rotationRadians) * smudgeDistance));

    const std::uint32_t localArgb = sampledSourceArgb(*context.sourceSampler, position);
    const std::uint32_t pulledArgb = sampledSourceArgb(*context.sourceSampler, {pulledX, pulledY});
    const UnitColor local = sourceAlpha(localArgb) > 0 ? unitColorFromArgb(localArgb) : brush;
    const UnitColor pulled = sourceAlpha(pulledArgb) > 0 ? unitColorFromArgb(pulledArgb) : local;
    const UnitColor dragged = mixColor(local, pulled, smudge);
    const UnitColor picked = mixColor(brush, dragged, pickup);
    const UnitColor pigment = mixColor(brush, picked, mix);
    const UnitColor depositedPigment = mixColor(pigment, brush, deposit);
    UnitColor result = mixColor(dragged, depositedPigment, std::max(deposit, pickup * mix));
    result.alpha = std::max(brush.alpha, std::max(local.alpha, pulled.alpha));
    return argbFromUnitColor(result);
}

std::uint8_t projectedAlpha(std::uint32_t argb, Types::Scalar maskAlpha, Types::Scalar alpha)
{
    const auto source = static_cast<Types::Scalar>(sourceAlpha(argb));
    const auto mask = clamp01(maskAlpha);
    return alphaByte(source * mask * clamp01(alpha));
}

std::uint8_t opacityCap(std::uint32_t argb,
                        Types::Scalar maskAlpha,
                        const Rasterizer &rasterizer,
                        Types::Scalar opacityCapScale)
{
    const auto source = static_cast<Types::Scalar>(sourceAlpha(argb));
    const auto mask = clamp01(maskAlpha);
    const Types::Scalar opacity = rasterizer.opacityEnabled ? rasterizer.opacity : 1.0;
    return alphaByte(source * mask * clamp01(opacity) * clamp01(opacityCapScale));
}

Types::Scalar applyMaskHardness(Types::Scalar maskAlpha, Types::Scalar hardness)
{
    const Types::Scalar clampedAlpha = clamp01(maskAlpha);
    if (clampedAlpha <= 0.0 || clampedAlpha >= 1.0) {
        return clampedAlpha;
    }

    const Types::Scalar clampedHardness = std::clamp(hardness, 0.01, 1.0);
    return std::pow(clampedAlpha, 1.0 / clampedHardness);
}

Types::Scalar circleMaskAlpha(const Rasterizer &rasterizer,
                              const BrushDab &dab,
                              Types::Pixel centerX,
                              Types::Pixel centerY,
                              Types::Pixel radius,
                              Types::Pixel x,
                              Types::Pixel y)
{
    if (radius <= 0) {
        return x == centerX && y == centerY ? 1.0 : 0.0;
    }

    const Types::Scalar dx = static_cast<Types::Scalar>(x - centerX);
    const Types::Scalar dy = static_cast<Types::Scalar>(y - centerY);
    const Types::Scalar distance = std::hypot(dx, dy);
    const Types::Scalar coverage = clamp01(static_cast<Types::Scalar>(radius) + 0.5 - distance);
    const Types::Scalar hardness = rasterizer.hardnessEnabled
            ? rasterizer.hardness * dab.hardnessScale
            : 1.0;
    return applyMaskHardness(coverage, hardness);
}

void appendCircle(std::vector<RasterSample> &samples,
                  Types::Pixel centerX,
                  Types::Pixel centerY,
                  Types::Pixel radius,
                  const BrushDab &dab,
                  const Rasterizer &rasterizer,
                  const ProjectionContext &context)
{
    const Types::Pixel clampedRadius = std::max<Types::Pixel>(0, radius);

    for (Types::Pixel y = centerY - clampedRadius; y <= centerY + clampedRadius; ++y) {
        for (Types::Pixel x = centerX - clampedRadius; x <= centerX + clampedRadius; ++x) {
            const Types::Scalar circleAlpha = circleMaskAlpha(rasterizer,
                                                              dab,
                                                              centerX,
                                                              centerY,
                                                              clampedRadius,
                                                              x,
                                                              y);
            if (circleAlpha <= 0.0) {
                continue;
            }

            const Types::Scalar maskAlpha = materialProjectionMask(circleAlpha, dab, {x, y}, context);
            if (maskAlpha <= 0.0) {
                continue;
            }
            const std::uint32_t colorArgb = wetDabColorArgb(dab, rasterizer, context, {x, y});
            const std::uint8_t alpha = projectedAlpha(colorArgb, maskAlpha, dab.alpha);
            const std::uint8_t cap = opacityCap(colorArgb, maskAlpha, rasterizer, dab.opacityCapScale);
            if (alpha == 0 || cap == 0) {
                continue;
            }
            const std::uint32_t argb = withAlpha(colorArgb, alpha);
            samples.push_back(RasterSample{{x, y}, argb, cap, dab.blendMode});
        }
    }
}

bool hasBrushImage(const Rasterizer &rasterizer)
{
    if (rasterizer.brushWidth <= 0 || rasterizer.brushHeight <= 0) {
        return false;
    }

    const auto expectedSize = static_cast<std::size_t>(rasterizer.brushWidth)
            * static_cast<std::size_t>(rasterizer.brushHeight);
    return rasterizer.brushAlpha.size() == expectedSize;
}

Types::Pixel roundedPixel(Types::Scalar value)
{
    return static_cast<Types::Pixel>(std::lround(value));
}

Types::Pixel floorPixel(Types::Scalar value)
{
    return static_cast<Types::Pixel>(std::floor(value));
}

Types::Pixel ceilPixel(Types::Scalar value)
{
    return static_cast<Types::Pixel>(std::ceil(value));
}

struct ScalarBounds {
    Types::Scalar left = 0.0;
    Types::Scalar top = 0.0;
    Types::Scalar right = 0.0;
    Types::Scalar bottom = 0.0;
};

DocumentRect documentRectFromBounds(ScalarBounds bounds)
{
    return DocumentRect{
            {bounds.left, bounds.top},
            std::max<Types::Scalar>(0.0, bounds.right - bounds.left),
            std::max<Types::Scalar>(0.0, bounds.bottom - bounds.top),
    };
}

DevicePixelRect deviceRectFromBounds(ScalarBounds bounds)
{
    const Types::Pixel left = floorPixel(bounds.left);
    const Types::Pixel top = floorPixel(bounds.top);
    const Types::Pixel right = ceilPixel(bounds.right);
    const Types::Pixel bottom = ceilPixel(bounds.bottom);
    return DevicePixelRect{
            {left, top},
            std::max<Types::Pixel>(0, right - left + 1),
            std::max<Types::Pixel>(0, bottom - top + 1),
    };
}

DocumentRect uniteDocumentRects(DocumentRect lhs, DocumentRect rhs)
{
    if (lhs.width <= 0.0 || lhs.height <= 0.0) {
        return rhs;
    }
    if (rhs.width <= 0.0 || rhs.height <= 0.0) {
        return lhs;
    }

    const Types::Scalar left = std::min(lhs.origin.x, rhs.origin.x);
    const Types::Scalar top = std::min(lhs.origin.y, rhs.origin.y);
    const Types::Scalar right = std::max(lhs.origin.x + lhs.width, rhs.origin.x + rhs.width);
    const Types::Scalar bottom = std::max(lhs.origin.y + lhs.height, rhs.origin.y + rhs.height);
    return DocumentRect{
            {left, top},
            std::max<Types::Scalar>(0.0, right - left),
            std::max<Types::Scalar>(0.0, bottom - top),
    };
}

BrushDab projectedDab(const BrushDab &dab, const RasterProjection &projection)
{
    const Types::Scalar scale = std::max<Types::Scalar>(0.01, projection.scale);
    BrushDab projected = dab;
    projected.position = DocumentPoint{
            static_cast<Types::Scalar>(projection.deviceOrigin.x)
                    + (dab.position.x - projection.documentOrigin.x) * scale,
            static_cast<Types::Scalar>(projection.deviceOrigin.y)
                    + (dab.position.y - projection.documentOrigin.y) * scale,
    };
    projected.scale *= scale;
    return projected;
}

Types::Scalar brushAlphaAt(const Rasterizer &rasterizer, Types::Pixel x, Types::Pixel y)
{
    if (x < 0 || y < 0 || x >= rasterizer.brushWidth || y >= rasterizer.brushHeight) {
        return 0.0;
    }

    const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(rasterizer.brushWidth)
            + static_cast<std::size_t>(x);
    return static_cast<Types::Scalar>(rasterizer.brushAlpha[index]) / 255.0;
}

Types::Scalar bilinearBrushAlphaAt(const Rasterizer &rasterizer,
                                   Types::Scalar sourceX,
                                   Types::Scalar sourceY)
{
    const auto x0 = static_cast<Types::Pixel>(std::floor(sourceX));
    const auto y0 = static_cast<Types::Pixel>(std::floor(sourceY));
    const Types::Scalar tx = sourceX - static_cast<Types::Scalar>(x0);
    const Types::Scalar ty = sourceY - static_cast<Types::Scalar>(y0);

    const Types::Scalar top = brushAlphaAt(rasterizer, x0, y0) * (1.0 - tx)
            + brushAlphaAt(rasterizer, x0 + 1, y0) * tx;
    const Types::Scalar bottom = brushAlphaAt(rasterizer, x0, y0 + 1) * (1.0 - tx)
            + brushAlphaAt(rasterizer, x0 + 1, y0 + 1) * tx;
    return top * (1.0 - ty) + bottom * ty;
}

Types::Scalar applyHardness(Types::Scalar maskAlpha, Types::Scalar hardness)
{
    return applyMaskHardness(maskAlpha, hardness);
}

Types::Scalar transformedBrushMaskAt(const Rasterizer &rasterizer,
                                     const BrushDab &dab,
                                     Types::Scalar canvasX,
                                     Types::Scalar canvasY,
                                     Types::Scalar scaleX,
                                     Types::Scalar scaleY,
                                     Types::Scalar cosTheta,
                                     Types::Scalar sinTheta,
                                     Types::Scalar centerOffsetX,
                                     Types::Scalar centerOffsetY)
{
    const Types::Scalar dx = canvasX - dab.position.x;
    const Types::Scalar dy = canvasY - dab.position.y;
    const Types::Scalar localX = (dx * cosTheta + dy * sinTheta) / scaleX;
    const Types::Scalar localY = (-dx * sinTheta + dy * cosTheta) / scaleY;
    const Types::Scalar sourceX = localX + centerOffsetX;
    const Types::Scalar sourceY = localY + centerOffsetY;
    const Types::Scalar hardness = rasterizer.hardnessEnabled
            ? rasterizer.hardness * dab.hardnessScale
            : 1.0;
    return applyHardness(bilinearBrushAlphaAt(rasterizer, sourceX, sourceY),
                         hardness);
}

Types::Scalar projectedBrushMaskAt(const Rasterizer &rasterizer,
                                   const BrushDab &dab,
                                   Types::Pixel x,
                                   Types::Pixel y,
                                   Types::Scalar scaleX,
                                   Types::Scalar scaleY,
                                   Types::Scalar cosTheta,
                                   Types::Scalar sinTheta,
                                   Types::Scalar centerOffsetX,
                                   Types::Scalar centerOffsetY)
{
    const Types::Scalar centerX = static_cast<Types::Scalar>(x);
    const Types::Scalar centerY = static_cast<Types::Scalar>(y);
    if (scaleX >= 1.0 && scaleY >= 1.0) {
        return transformedBrushMaskAt(rasterizer,
                                      dab,
                                      centerX,
                                      centerY,
                                      scaleX,
                                      scaleY,
                                      cosTheta,
                                      sinTheta,
                                      centerOffsetX,
                                      centerOffsetY);
    }

    const std::array<CanvasPoint, 5> offsets{{
            {0.0, 0.0},
            {-0.25, -0.25},
            {0.25, -0.25},
            {-0.25, 0.25},
            {0.25, 0.25},
    }};
    Types::Scalar maskAlpha = 0.0;
    for (const CanvasPoint offset : offsets) {
        maskAlpha += transformedBrushMaskAt(rasterizer,
                                            dab,
                                            centerX + offset.x,
                                            centerY + offset.y,
                                            scaleX,
                                            scaleY,
                                            cosTheta,
                                            sinTheta,
                                            centerOffsetX,
                                            centerOffsetY);
    }

    return maskAlpha / static_cast<Types::Scalar>(offsets.size());
}

void includePoint(Types::Scalar x,
                  Types::Scalar y,
                  Types::Scalar &left,
                  Types::Scalar &top,
                  Types::Scalar &right,
                  Types::Scalar &bottom)
{
    left = std::min(left, x);
    top = std::min(top, y);
    right = std::max(right, x);
    bottom = std::max(bottom, y);
}

Types::Scalar circleRadius(const BrushDab &dab, const Rasterizer &rasterizer)
{
    return std::max<Types::Scalar>(0.0, std::lround(static_cast<Types::Scalar>(rasterizer.radius) * dab.scale));
}

ScalarBounds circleBoundsForDab(const BrushDab &dab, const Rasterizer &rasterizer)
{
    const Types::Pixel centerX = roundedPixel(dab.position.x);
    const Types::Pixel centerY = roundedPixel(dab.position.y);
    const Types::Scalar radius = circleRadius(dab, rasterizer);
    return ScalarBounds{
            static_cast<Types::Scalar>(centerX) - radius,
            static_cast<Types::Scalar>(centerY) - radius,
            static_cast<Types::Scalar>(centerX) + radius,
            static_cast<Types::Scalar>(centerY) + radius,
    };
}

ScalarBounds brushImageBoundsForDab(const BrushDab &dab, const Rasterizer &rasterizer)
{
    const Types::Scalar centerOffsetX = static_cast<Types::Scalar>(rasterizer.brushWidth - 1) * 0.5;
    const Types::Scalar centerOffsetY = static_cast<Types::Scalar>(rasterizer.brushHeight - 1) * 0.5;
    const Types::Scalar sourceLeft = -centerOffsetX - 0.5;
    const Types::Scalar sourceRight = static_cast<Types::Scalar>(rasterizer.brushWidth - 1) - centerOffsetX + 0.5;
    const Types::Scalar sourceTop = -centerOffsetY - 0.5;
    const Types::Scalar sourceBottom = static_cast<Types::Scalar>(rasterizer.brushHeight - 1) - centerOffsetY + 0.5;
    const Types::Scalar scaleX = std::max<Types::Scalar>(0.01, dab.scale * dab.ellipseScaleX);
    const Types::Scalar scaleY = std::max<Types::Scalar>(0.01, dab.scale * dab.ellipseScaleY);
    const Types::Scalar cosTheta = std::cos(dab.rotationRadians);
    const Types::Scalar sinTheta = std::sin(dab.rotationRadians);

    Types::Scalar left = dab.position.x;
    Types::Scalar top = dab.position.y;
    Types::Scalar right = dab.position.x;
    Types::Scalar bottom = dab.position.y;
    const std::array<CanvasPoint, 4> sourceCorners{{
            {sourceLeft, sourceTop},
            {sourceRight, sourceTop},
            {sourceRight, sourceBottom},
            {sourceLeft, sourceBottom},
    }};
    for (const CanvasPoint sourceCorner : sourceCorners) {
        const Types::Scalar localX = sourceCorner.x * scaleX;
        const Types::Scalar localY = sourceCorner.y * scaleY;
        const Types::Scalar rotatedX = localX * cosTheta - localY * sinTheta;
        const Types::Scalar rotatedY = localX * sinTheta + localY * cosTheta;
        includePoint(dab.position.x + rotatedX, dab.position.y + rotatedY, left, top, right, bottom);
    }

    return ScalarBounds{left, top, right, bottom};
}

ScalarBounds boundsForDab(const BrushDab &dab, const Rasterizer &rasterizer)
{
    if (hasBrushImage(rasterizer)) {
        return brushImageBoundsForDab(dab, rasterizer);
    }

    return circleBoundsForDab(dab, rasterizer);
}

void appendBrushImage(std::vector<RasterSample> &samples,
                      const BrushDab &dab,
                      const Rasterizer &rasterizer,
                      const ProjectionContext &context)
{
    const Types::Scalar centerOffsetX = static_cast<Types::Scalar>(rasterizer.brushWidth - 1) * 0.5;
    const Types::Scalar centerOffsetY = static_cast<Types::Scalar>(rasterizer.brushHeight - 1) * 0.5;
    const Types::Scalar sourceLeft = -centerOffsetX - 0.5;
    const Types::Scalar sourceRight = static_cast<Types::Scalar>(rasterizer.brushWidth - 1) - centerOffsetX + 0.5;
    const Types::Scalar sourceTop = -centerOffsetY - 0.5;
    const Types::Scalar sourceBottom = static_cast<Types::Scalar>(rasterizer.brushHeight - 1) - centerOffsetY + 0.5;
    const Types::Scalar scaleX = std::max<Types::Scalar>(0.01, dab.scale * dab.ellipseScaleX);
    const Types::Scalar scaleY = std::max<Types::Scalar>(0.01, dab.scale * dab.ellipseScaleY);
    const Types::Scalar cosTheta = std::cos(dab.rotationRadians);
    const Types::Scalar sinTheta = std::sin(dab.rotationRadians);

    Types::Scalar left = dab.position.x;
    Types::Scalar top = dab.position.y;
    Types::Scalar right = dab.position.x;
    Types::Scalar bottom = dab.position.y;

    const std::array<CanvasPoint, 4> sourceCorners{{
                 {sourceLeft, sourceTop},
                 {sourceRight, sourceTop},
                 {sourceRight, sourceBottom},
                 {sourceLeft, sourceBottom},
    }};
    for (const CanvasPoint sourceCorner : sourceCorners) {
        const Types::Scalar localX = sourceCorner.x * scaleX;
        const Types::Scalar localY = sourceCorner.y * scaleY;
        const Types::Scalar rotatedX = localX * cosTheta - localY * sinTheta;
        const Types::Scalar rotatedY = localX * sinTheta + localY * cosTheta;
        includePoint(dab.position.x + rotatedX, dab.position.y + rotatedY, left, top, right, bottom);
    }

    const Types::Pixel minX = static_cast<Types::Pixel>(std::floor(left));
    const Types::Pixel minY = static_cast<Types::Pixel>(std::floor(top));
    const Types::Pixel maxX = static_cast<Types::Pixel>(std::ceil(right));
    const Types::Pixel maxY = static_cast<Types::Pixel>(std::ceil(bottom));

    for (Types::Pixel y = minY; y <= maxY; ++y) {
        for (Types::Pixel x = minX; x <= maxX; ++x) {
            const Types::Scalar maskAlpha = projectedBrushMaskAt(rasterizer,
                                                                 dab,
                                                                 x,
                                                                 y,
                                                                 scaleX,
                                                                 scaleY,
                                                                 cosTheta,
                                                                 sinTheta,
                                                                 centerOffsetX,
                                                                 centerOffsetY);
            if (maskAlpha <= 0.0) {
                continue;
            }

            const Types::Scalar materialMaskAlpha = materialProjectionMask(maskAlpha, dab, {x, y}, context);
            if (materialMaskAlpha <= 0.0) {
                continue;
            }
            const std::uint32_t colorArgb = wetDabColorArgb(dab, rasterizer, context, {x, y});
            const std::uint8_t alpha = projectedAlpha(colorArgb, materialMaskAlpha, dab.alpha);
            const std::uint8_t cap = opacityCap(colorArgb, materialMaskAlpha, rasterizer, dab.opacityCapScale);
            if (alpha == 0 || cap == 0) {
                continue;
            }

            samples.push_back(RasterSample{
                    {x, y},
                    withAlpha(colorArgb, alpha),
                    cap,
                    dab.blendMode,
            });
        }
    }
}

void appendBrushProjection(std::vector<RasterSample> &samples,
                           const BrushDab &dab,
                           const Rasterizer &rasterizer,
                           const ProjectionContext &context)
{
    const Types::Pixel centerX = roundedPixel(dab.position.x);
    const Types::Pixel centerY = roundedPixel(dab.position.y);
    if (hasBrushImage(rasterizer)) {
        appendBrushImage(samples, dab, rasterizer, context);
    } else {
        const auto radius = static_cast<Types::Pixel>(
                std::max<Types::Scalar>(0.0, std::lround(static_cast<Types::Scalar>(rasterizer.radius) * dab.scale)));
        appendCircle(samples, centerX, centerY, radius, dab, rasterizer, context);
    }
}

StrokePoint interpolateSample(const StrokePoint &start, const StrokePoint &end, Types::Scalar t)
{
    return StrokePoint{
            {start.position.x + (end.position.x - start.position.x) * t,
             start.position.y + (end.position.y - start.position.y) * t},
            start.pressure + (end.pressure - start.pressure) * t,
            start.time + (end.time - start.time) * t,
            start.velocity + (end.velocity - start.velocity) * t,
            start.tiltX + (end.tiltX - start.tiltX) * t,
            start.tiltY + (end.tiltY - start.tiltY) * t,
            t < 1.0 ? start.deviceState : end.deviceState,
            start.arcLength + (end.arcLength - start.arcLength) * t,
            start.rotationRadians + (end.rotationRadians - start.rotationRadians) * t,
    };
}

Types::Scalar effectiveSpacing(const Rasterizer &rasterizer,
                               const BrushDynamics &dynamics,
                               const StrokePoint &sample)
{
    const Types::Scalar density = std::max<Types::Scalar>(0.01, rasterizer.density);
    const Types::Scalar velocity = dynamics.velocityInputEnabled ? std::max<Types::Scalar>(0.0, sample.velocity) : 0.0;
    const Types::Scalar velocityScale = 1.0 + velocity * rasterizer.velocitySpacing;
    const Types::Scalar baseSpacing = rasterizer.spacingEnabled
            ? (rasterizer.brushSize > 0.0
                       ? rasterizer.brushSize * std::max<Types::Scalar>(0.01, rasterizer.spacingRatio)
                       : rasterizer.spacing)
            : 0.0;
    const BrushDynamicsResult dynamicsResult = resolveBrushDynamics(
            dynamics,
            BrushDynamicsInput{sample.pressure, sample.velocity, sample.tiltX, sample.tiltY});
    return std::max<Types::Scalar>(
            0.01,
            baseSpacing * std::max<Types::Scalar>(0.01, velocityScale) * dynamicsResult.spacingScale / density);
}

Types::Scalar totalCurveLength(const StrokeCurve &curve)
{
    Types::Scalar totalLength = 0.0;
    for (std::size_t index = 0; index + 1 < curve.samples.size(); ++index) {
        const StrokePoint &start = curve.samples[index];
        const StrokePoint &end = curve.samples[index + 1];
        totalLength += std::hypot(end.position.x - start.position.x, end.position.y - start.position.y);
    }
    return totalLength;
}

Types::Scalar deterministicUnit(std::uint32_t randomSeed, std::uint32_t sequenceIndex)
{
    std::uint32_t value = randomSeed ^ (sequenceIndex * 0x9E3779B9U);
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return static_cast<Types::Scalar>(value) / static_cast<Types::Scalar>(0xFFFFFFFFU);
}

Types::Scalar deterministicSigned(std::uint32_t randomSeed, std::uint32_t sequenceIndex)
{
    return deterministicUnit(randomSeed, sequenceIndex) * 2.0 - 1.0;
}

Types::Pixel textureWidth(const BrushTexture &texture)
{
    return texture.assetCache.enabled ? texture.assetCache.width : texture.width;
}

Types::Pixel textureHeight(const BrushTexture &texture)
{
    return texture.assetCache.enabled ? texture.assetCache.height : texture.height;
}

const std::vector<Types::Byte> &textureAlphaBuffer(const BrushTexture &texture)
{
    return texture.assetCache.enabled ? texture.assetCache.alpha : texture.alpha;
}

bool hasTextureAlpha(const BrushTexture &texture)
{
    const Types::Pixel width = textureWidth(texture);
    const Types::Pixel height = textureHeight(texture);
    const std::vector<Types::Byte> &alpha = textureAlphaBuffer(texture);
    return texture.enabled
            && width > 0
            && height > 0
            && alpha.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

Types::Pixel wrappedPixel(Types::Scalar value, Types::Pixel size)
{
    if (size <= 0) {
        return 0;
    }
    Types::Pixel pixel = static_cast<Types::Pixel>(std::floor(value)) % size;
    if (pixel < 0) {
        pixel += size;
    }
    return pixel;
}

Types::Scalar textureAlphaAtIndex(const BrushTexture &texture, Types::Scalar u, Types::Scalar v)
{
    if (!hasTextureAlpha(texture)) {
        return 1.0;
    }

    const Types::Pixel width = textureWidth(texture);
    const Types::Pixel height = textureHeight(texture);
    const Types::Pixel x = wrappedPixel(u, width);
    const Types::Pixel y = wrappedPixel(v, height);
    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
    return static_cast<Types::Scalar>(textureAlphaBuffer(texture)[index]) / 255.0;
}

Types::Scalar materialTextureAlpha(const BrushTexture &texture, std::uint32_t sequenceIndex)
{
    if (!hasTextureAlpha(texture)) {
        return 1.0;
    }

    const std::vector<Types::Byte> &alpha = textureAlphaBuffer(texture);
    const std::size_t index = static_cast<std::size_t>(sequenceIndex) % alpha.size();
    return static_cast<Types::Scalar>(alpha[index]) / 255.0;
}

DocumentPoint documentPointForDevice(DevicePixelPoint position, const ProjectionContext &context)
{
    const Types::Scalar scale = std::max<Types::Scalar>(0.01, context.projection.scale);
    return DocumentPoint{
            context.projection.documentOrigin.x
                    + (static_cast<Types::Scalar>(position.x) - static_cast<Types::Scalar>(context.projection.deviceOrigin.x))
                            / scale,
            context.projection.documentOrigin.y
                    + (static_cast<Types::Scalar>(position.y) - static_cast<Types::Scalar>(context.projection.deviceOrigin.y))
                            / scale,
    };
}

Types::Scalar sampledTextureAlpha(const BrushTexture &texture,
                                  const BrushDab &dab,
                                  DevicePixelPoint position,
                                  const ProjectionContext &context)
{
    if (!hasTextureAlpha(texture)) {
        return 1.0;
    }

    const Types::Scalar textureScale = std::max<Types::Scalar>(0.01, texture.scale * dab.textureScale);
    const Types::Scalar rotation = dab.rotationRadians + texture.rotationRadians + dab.textureRotationRadians;
    const Types::Scalar cosTheta = std::cos(rotation);
    const Types::Scalar sinTheta = std::sin(rotation);
    Types::Scalar u = 0.0;
    Types::Scalar v = 0.0;
    switch (texture.space) {
        case BrushTextureSpace::Document:
        case BrushTextureSpace::Paper: {
            const DocumentPoint document = documentPointForDevice(position, context);
            u = document.x / textureScale;
            v = document.y / textureScale;
            break;
        }
        case BrushTextureSpace::StrokeFollow: {
            const Types::Scalar dx = static_cast<Types::Scalar>(position.x) - dab.position.x;
            const Types::Scalar dy = static_cast<Types::Scalar>(position.y) - dab.position.y;
            u = (dab.strokeDistance + dx * cosTheta + dy * sinTheta) / textureScale;
            v = (-dx * sinTheta + dy * cosTheta) / textureScale;
            break;
        }
        case BrushTextureSpace::Tip: {
            const Types::Scalar dx = static_cast<Types::Scalar>(position.x) - dab.position.x;
            const Types::Scalar dy = static_cast<Types::Scalar>(position.y) - dab.position.y;
            u = (dx * cosTheta + dy * sinTheta) / textureScale;
            v = (-dx * sinTheta + dy * cosTheta) / textureScale;
            break;
        }
    }
    return textureAlphaAtIndex(texture, u + texture.offsetX, v + texture.offsetY);
}

Types::Scalar applyTextureMask(Types::Scalar maskAlpha,
                               const BrushTexture &texture,
                               const BrushDab &dab,
                               DevicePixelPoint position,
                               const ProjectionContext &context)
{
    if (!hasTextureAlpha(texture)) {
        return maskAlpha;
    }

    const Types::Scalar textureAlpha = sampledTextureAlpha(texture, dab, position, context);
    const Types::Scalar strength = clamp01(texture.strength);
    return maskAlpha * (1.0 - strength * (1.0 - textureAlpha));
}

Types::Scalar dualBrushAlphaAt(const DualBrush &dualBrush, const BrushDab &dab, DevicePixelPoint position)
{
    if (!dualBrush.enabled
            || dualBrush.width <= 0
            || dualBrush.height <= 0
            || dualBrush.alpha.size()
                    != static_cast<std::size_t>(dualBrush.width) * static_cast<std::size_t>(dualBrush.height)) {
        return 1.0;
    }

    const Types::Scalar scale = std::max<Types::Scalar>(0.01, dualBrush.scale * dab.dualBrushScale);
    const Types::Scalar rotation = dab.rotationRadians + dualBrush.rotationRadians + dab.dualBrushRotationRadians;
    const Types::Scalar cosTheta = std::cos(rotation);
    const Types::Scalar sinTheta = std::sin(rotation);
    const Types::Scalar dx = static_cast<Types::Scalar>(position.x) - dab.position.x;
    const Types::Scalar dy = static_cast<Types::Scalar>(position.y) - dab.position.y;
    const Types::Scalar u = (dx * cosTheta + dy * sinTheta) / scale + dualBrush.offsetX;
    const Types::Scalar v = (-dx * sinTheta + dy * cosTheta) / scale + dualBrush.offsetY;
    const Types::Pixel x = wrappedPixel(u, dualBrush.width);
    const Types::Pixel y = wrappedPixel(v, dualBrush.height);
    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(dualBrush.width)
            + static_cast<std::size_t>(x);
    return static_cast<Types::Scalar>(dualBrush.alpha[index]) / 255.0;
}

Types::Scalar applyDualBrushMask(Types::Scalar maskAlpha,
                                 const DualBrush &dualBrush,
                                 const BrushDab &dab,
                                 DevicePixelPoint position)
{
    if (!dualBrush.enabled) {
        return maskAlpha;
    }

    const Types::Scalar dualAlpha = dualBrushAlphaAt(dualBrush, dab, position) * clamp01(dualBrush.opacity);
    switch (dualBrush.compositeMode) {
        case DualBrushCompositeMode::Add:
            return clamp01(maskAlpha + dualAlpha);
        case DualBrushCompositeMode::Subtract:
            return clamp01(maskAlpha * (1.0 - dualAlpha));
        case DualBrushCompositeMode::Difference:
            return clamp01(std::abs(maskAlpha - dualAlpha));
        case DualBrushCompositeMode::Multiply:
            return clamp01(maskAlpha * dualAlpha);
    }
    return maskAlpha;
}

Types::Scalar materialProjectionMask(Types::Scalar maskAlpha,
                                     const BrushDab &dab,
                                     DevicePixelPoint position,
                                     const ProjectionContext &context)
{
    if (context.material == nullptr) {
        return maskAlpha;
    }

    Types::Scalar result = maskAlpha;
    result = applyTextureMask(result, context.material->texture, dab, position, context);
    result = applyTextureMask(result, context.material->paperGrain, dab, position, context);
    result = applyDualBrushMask(result, context.material->dualBrush, dab, position);
    return clamp01(result);
}

Types::Scalar materialFlowScale(const BrushMaterial &material,
                                Types::Scalar textureAlpha,
                                Types::Scalar textureDepthScale,
                                Types::Scalar wetnessScale,
                                Types::Scalar dryOutScale)
{
    Types::Scalar scale = 1.0;
    if (material.texture.enabled) {
        const Types::Scalar textureDepth = std::clamp(material.texture.grainStrength * textureDepthScale, 0.0, 1.0);
        scale *= 1.0 - textureDepth * (1.0 - textureAlpha);
    }
    if (material.dualBrush.enabled) {
        scale *= std::clamp(material.dualBrush.scale, 0.0, 1.0);
    }
    if (material.simulation.enabled) {
        const Types::Scalar wetness = std::clamp(material.simulation.wetness * wetnessScale * dryOutScale, 0.0, 1.0);
        scale *= 1.0 - wetness * 0.25;
        scale *= 1.0 - std::clamp(material.simulation.smudgeStrength, 0.0, 1.0) * 0.15;
        scale *= 1.0 - std::clamp(material.simulation.mixStrength, 0.0, 1.0) * 0.10;
    }
    return std::clamp(scale * dryOutScale, 0.0, 1.0);
}

DocumentPoint scatterPosition(DocumentPoint position,
                              const BrushScatter &scatter,
                              Types::Scalar scatterScale,
                              std::uint32_t randomSeed,
                              std::uint32_t sequenceIndex)
{
    if (!scatter.enabled || scatter.radius <= 0.0 || scatterScale <= 0.0) {
        return position;
    }

    const Types::Scalar radius = scatter.radius * scatterScale;
    const Types::Scalar dx = deterministicSigned(randomSeed + 0x51A7U, sequenceIndex) * radius;
    const Types::Scalar dy = deterministicSigned(randomSeed + 0x8D31U, sequenceIndex) * radius;
    return {position.x + dx, position.y + dy};
}

Types::Scalar taperFactor(const Rasterizer &rasterizer, Types::Scalar distanceOnCurve, Types::Scalar curveLength)
{
    const auto shapedProgress = [](StrokeTaperShape shape, Types::Scalar progress) {
        const Types::Scalar t = clamp01(progress);
        switch (shape) {
            case StrokeTaperShape::EaseIn:
                return t * t;
            case StrokeTaperShape::EaseOut:
                return 1.0 - (1.0 - t) * (1.0 - t);
            case StrokeTaperShape::SmoothStep:
                return t * t * (3.0 - 2.0 * t);
            case StrokeTaperShape::Linear:
                return t;
        }
        return t;
    };
    const Types::Scalar minimum = clamp01(rasterizer.taperMinimum);
    const auto tapered = [minimum, shapedProgress](StrokeTaperShape shape, Types::Scalar progress) {
        return minimum + shapedProgress(shape, progress) * (1.0 - minimum);
    };

    Types::Scalar factor = 1.0;
    if (rasterizer.warmupDistance > 0.0) {
        const Types::Scalar warmup = std::clamp(distanceOnCurve / rasterizer.warmupDistance, 0.0, 1.0);
        factor = std::min(factor, tapered(rasterizer.warmupTaperShape, warmup));
    }
    if (rasterizer.taperDistance > 0.0) {
        const Types::Scalar remaining = std::max<Types::Scalar>(0.0, curveLength - distanceOnCurve);
        const Types::Scalar taper = std::clamp(remaining / rasterizer.taperDistance, 0.0, 1.0);
        factor = std::min(factor, tapered(rasterizer.endTaperShape, taper));
    }
    return factor;
}

Types::Scalar bristleCountScale(const BristleSimulation &bristle)
{
    if (!bristle.enabled || bristle.count == 0U) {
        return 0.0;
    }
    return std::clamp(static_cast<Types::Scalar>(bristle.count) / 16.0, 0.0, 1.0);
}

Types::Scalar bristleLengthScale(const BristleSimulation &bristle, Types::Scalar spreadScale)
{
    if (!bristle.enabled || bristle.length <= 0.0) {
        return 0.0;
    }
    return std::clamp((bristle.length * spreadScale) / 10.0, 0.0, 1.0);
}

Types::Scalar bristleEllipseScaleX(const BristleSimulation &bristle, Types::Scalar spreadScale)
{
    if (!bristle.enabled) {
        return 1.0;
    }

    const Types::Scalar count = bristleCountScale(bristle);
    const Types::Scalar length = bristleLengthScale(bristle, spreadScale);
    switch (bristle.shape) {
        case BristleShape::Flat:
            return 1.0 + count * 0.35 + length * 0.45;
        case BristleShape::Fan:
            return 1.0 + count * 0.5 + length * 0.65;
        case BristleShape::Round:
            return 1.0 + length * 0.1;
    }
    return 1.0;
}

Types::Scalar bristleEllipseScaleY(const BristleSimulation &bristle, Types::Scalar spreadScale)
{
    if (!bristle.enabled) {
        return 1.0;
    }

    const Types::Scalar stiffness = clamp01(bristle.stiffness);
    const Types::Scalar spread = std::max<Types::Scalar>(0.0, spreadScale);
    switch (bristle.shape) {
        case BristleShape::Flat:
            return std::max<Types::Scalar>(0.25, 0.85 - (1.0 - stiffness) * 0.45 * spread);
        case BristleShape::Fan:
            return std::max<Types::Scalar>(0.2, 0.95 - (1.0 - stiffness) * 0.55 * spread);
        case BristleShape::Round:
            return 1.0;
    }
    return 1.0;
}

BrushDab makeBrushDab(const StrokePoint &sample,
                      Types::Scalar tangentRadians,
                      const Rasterizer &rasterizer,
                      const BrushDynamics &dynamics,
                      const BrushMaterial &material,
                      Types::Scalar distanceOnCurve,
                      Types::Scalar curveLength,
                      std::uint32_t randomSeed,
                      std::uint32_t sequenceIndex)
{
    const Types::Scalar pressure = dynamics.pressureInputEnabled ? clamp01(sample.pressure) : 1.0;
    const Types::Scalar scale = std::max<Types::Scalar>(
            0.01,
            1.0 + (pressure - 1.0) * rasterizer.pressureScale);
    const Types::Scalar tiltX = dynamics.tiltInputEnabled ? sample.tiltX : 0.0;
    const Types::Scalar tiltY = dynamics.tiltInputEnabled ? sample.tiltY : 0.0;
    const bool hasTilt = tiltX != 0.0 || tiltY != 0.0;
    const BrushDynamicsResult dynamicsResult = resolveBrushDynamics(
            dynamics,
            BrushDynamicsInput{
                    sample.pressure,
                    sample.velocity,
                    sample.tiltX,
                    sample.tiltY,
                    deterministicSigned(randomSeed, sequenceIndex),
                    deterministicUnit(randomSeed + 0xA511E9B3U, sequenceIndex),
            });
    const Types::Scalar rasterizerJitter = dynamics.randomInputEnabled
            ? deterministicSigned(randomSeed, sequenceIndex) * rasterizer.rotationJitter
            : 0.0;
    const Types::Scalar jitter = rasterizerJitter
            + dynamicsResult.rotationJitterRadians;
    const Types::Scalar baseRotation = dynamicsResult.rotationFromTilt
            ? dynamicsResult.rotationRadians
            : (hasTilt ? std::atan2(tiltY, tiltX) : tangentRadians);
    const Types::Scalar textureDirection = dynamicsResult.textureDirectionFromTilt
            ? dynamicsResult.textureDirectionRadians
            : baseRotation;
    const Types::Scalar textureAlpha = materialTextureAlpha(material.texture, sequenceIndex);
    const Types::Scalar textureScaleJitter = dynamics.randomInputEnabled
            ? deterministicSigned(randomSeed + 0x4D27U, sequenceIndex) * material.texture.scaleJitter
            : 0.0;
    const Types::Scalar textureRotationJitter = dynamics.randomInputEnabled
            ? deterministicSigned(randomSeed + 0x9AC1U, sequenceIndex) * material.texture.rotationJitter
            : 0.0;
    const Types::Scalar dualBrushScaleJitter = dynamics.randomInputEnabled
            ? deterministicSigned(randomSeed + 0x6E15U, sequenceIndex) * material.dualBrush.scaleJitter
            : 0.0;
    const Types::Scalar dualBrushRotationJitter = dynamics.randomInputEnabled
            ? deterministicSigned(randomSeed + 0xB53DU, sequenceIndex) * material.dualBrush.rotationJitter
            : 0.0;
    const DocumentPoint position = scatterPosition(sample.position,
                                                   material.scatter,
                                                   dynamicsResult.scatterScale,
                                                   randomSeed,
                                                   sequenceIndex);
    const Types::Scalar flow = rasterizer.flowEnabled ? rasterizer.flow : 1.0;

    BrushDab dab;
    dab.position = position;
    dab.scale = scale * dynamicsResult.sizeScale
            * (material.dualBrush.enabled ? std::max<Types::Scalar>(0.01, material.dualBrush.scale) : 1.0);
    dab.rotationRadians = baseRotation + jitter + dynamicsResult.rotationOffsetRadians;
    dab.alpha = clamp01(flow) * dynamicsResult.flowScale * taperFactor(rasterizer, distanceOnCurve, curveLength)
            * materialFlowScale(material,
                                textureAlpha,
                                dynamicsResult.textureDepthScale,
                                dynamicsResult.wetnessScale,
                                dynamicsResult.dryOutScale);
    dab.opacityCapScale = dynamicsResult.opacityScale;
    dab.hardnessScale = dynamicsResult.hardnessScale;
    dab.ellipseScaleX = dynamicsResult.ellipseScaleX
            * bristleEllipseScaleX(material.bristle, dynamicsResult.bristleSpreadScale);
    dab.ellipseScaleY = dynamicsResult.ellipseScaleY
            * bristleEllipseScaleY(material.bristle, dynamicsResult.bristleSpreadScale);
    dab.textureDirectionRadians = textureDirection;
    dab.grain = clamp01(dynamicsResult.grain
                        + material.texture.grainStrength * dynamicsResult.textureDepthScale * (1.0 - textureAlpha));
    dab.textureAlpha = textureAlpha;
    dab.textureDepthScale = dynamicsResult.textureDepthScale;
    dab.textureScale = std::max<Types::Scalar>(0.01, 1.0 + textureScaleJitter);
    dab.textureRotationRadians = textureRotationJitter;
    dab.wetnessScale = dynamicsResult.wetnessScale;
    dab.dryOutScale = dynamicsResult.dryOutScale;
    dab.bristleSpreadScale = dynamicsResult.bristleSpreadScale;
    dab.scatterScale = dynamicsResult.scatterScale;
    dab.dualBrushScale = std::max<Types::Scalar>(0.01, 1.0 + dualBrushScaleJitter);
    dab.dualBrushRotationRadians = dualBrushRotationJitter;
    dab.strokeDistance = distanceOnCurve;
    dab.dualBrush = material.dualBrush.enabled;
    dab.colorArgb = rasterizer.argb;
    dab.blendMode = rasterizer.blendMode;
    dab.sequenceIndex = sequenceIndex;
    return dab;
}

void appendDabAtDistance(std::vector<BrushDab> &dabs,
                         const StrokePoint &start,
                         const StrokePoint &end,
                         Types::Scalar distanceWithinSegment,
                         Types::Scalar segmentLength,
                         Types::Scalar distanceOnCurve,
                         Types::Scalar curveLength,
                         const Rasterizer &rasterizer,
                         const BrushDynamics &dynamics,
                         const BrushMaterial &material,
                         std::uint32_t randomSeed)
{
    const Types::Scalar dx = end.position.x - start.position.x;
    const Types::Scalar dy = end.position.y - start.position.y;
    const Types::Scalar t = segmentLength > 0.0 ? distanceWithinSegment / segmentLength : 0.0;
    const StrokePoint sample = interpolateSample(start, end, std::clamp(t, 0.0, 1.0));
    const auto sequenceIndex = static_cast<std::uint32_t>(dabs.size());
    dabs.push_back(makeBrushDab(sample,
                                std::atan2(dy, dx),
                                rasterizer,
                                dynamics,
                                material,
                                distanceOnCurve,
                                curveLength,
                                randomSeed,
                                sequenceIndex));
}

} // namespace

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve, const Rasterizer &rasterizer)
{
    return placeBrushDabs(curve, rasterizer, 0);
}

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     std::uint32_t randomSeed)
{
    return placeBrushDabs(curve, rasterizer, BrushDynamics{}, randomSeed);
}

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     const BrushDynamics &dynamics,
                                     std::uint32_t randomSeed)
{
    return placeBrushDabs(curve, rasterizer, dynamics, BrushMaterial{}, randomSeed);
}

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve,
                                     const Rasterizer &rasterizer,
                                     const BrushDynamics &dynamics,
                                     const BrushMaterial &material,
                                     std::uint32_t randomSeed)
{
    std::vector<BrushDab> dabs;
    if (curve.samples.empty()) {
        return dabs;
    }

    if (curve.samples.size() == 1) {
        dabs.push_back(makeBrushDab(curve.samples.front(), 0.0, rasterizer, dynamics, material, 0.0, 0.0, randomSeed, 0));
        return dabs;
    }

    const Types::Scalar curveLength = totalCurveLength(curve);
    Types::Scalar segmentStartDistance = 0.0;
    Types::Scalar nextDabDistance = 0.0;
    Types::Scalar lastPlacedDistance = -1.0;
    constexpr Types::Scalar epsilon = 0.000001;

    for (std::size_t index = 0; index + 1 < curve.samples.size(); ++index) {
        const StrokePoint &start = curve.samples[index];
        const StrokePoint &end = curve.samples[index + 1];
        const Types::Scalar segmentLength = std::hypot(end.position.x - start.position.x,
                                                       end.position.y - start.position.y);
        if (segmentLength <= 0.0) {
            continue;
        }

        const Types::Scalar segmentEndDistance = segmentStartDistance + segmentLength;
        while (nextDabDistance <= segmentEndDistance + epsilon) {
            if (nextDabDistance + epsilon >= segmentStartDistance) {
                const Types::Scalar distanceWithinSegment = nextDabDistance - segmentStartDistance;
                const StrokePoint sample = interpolateSample(start,
                                                             end,
                                                             std::clamp(distanceWithinSegment / segmentLength,
                                                                        0.0,
                                                                        1.0));
                appendDabAtDistance(dabs,
                                    start,
                                    end,
                                    distanceWithinSegment,
                                    segmentLength,
                                    nextDabDistance,
                                    curveLength,
                                    rasterizer,
                                    dynamics,
                                    material,
                                    randomSeed);
                lastPlacedDistance = nextDabDistance;
                nextDabDistance += effectiveSpacing(rasterizer, dynamics, sample);
            } else {
                nextDabDistance += effectiveSpacing(rasterizer, dynamics, start);
            }
        }

        segmentStartDistance = segmentEndDistance;
    }

    if (dabs.empty() || curveLength - lastPlacedDistance > epsilon) {
        const StrokePoint &previous = curve.samples[curve.samples.size() - 2];
        const StrokePoint &last = curve.samples.back();
        appendDabAtDistance(dabs,
                            previous,
                            last,
                            std::hypot(last.position.x - previous.position.x,
                                       last.position.y - previous.position.y),
                            std::hypot(last.position.x - previous.position.x,
                                       last.position.y - previous.position.y),
                            curveLength,
                            curveLength,
                            rasterizer,
                            dynamics,
                            material,
                            randomSeed);
    }

    return dabs;
}

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer)
{
    return projectBrushDabs(std::span<const BrushDab>{dabs.data(), dabs.size()}, rasterizer);
}

std::vector<RasterSample> projectBrushDabs(std::span<const BrushDab> dabs, const Rasterizer &rasterizer)
{
    return projectBrushDabs(dabs, rasterizer, RasterProjection{});
}

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection)
{
    return projectBrushDabs(std::span<const BrushDab>{dabs.data(), dabs.size()}, rasterizer, projection);
}

std::vector<RasterSample> projectBrushDabs(std::span<const BrushDab> dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection)
{
    std::vector<RasterSample> samples;
    samples.reserve(projectedSampleCapacity(dabs, rasterizer));
    ProjectionContext context;
    context.projection = projection;
    for (const BrushDab &dab : dabs) {
        appendBrushProjection(samples, projectedDab(dab, projection), rasterizer, context);
    }

    return samples;
}

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const BrushMaterial &material)
{
    return projectBrushDabs(std::span<const BrushDab>{dabs.data(), dabs.size()},
                            rasterizer,
                            projection,
                            material);
}

std::vector<RasterSample> projectBrushDabs(std::span<const BrushDab> dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const BrushMaterial &material)
{
    std::vector<RasterSample> samples;
    samples.reserve(projectedSampleCapacity(dabs, rasterizer));
    ProjectionContext context;
    context.material = &material;
    context.projection = projection;
    for (const BrushDab &dab : dabs) {
        appendBrushProjection(samples, projectedDab(dab, projection), rasterizer, context);
    }

    return samples;
}

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const RasterSourceSampler &sourceSampler,
                                           const BrushMaterial &material)
{
    return projectBrushDabs(std::span<const BrushDab>{dabs.data(), dabs.size()},
                            rasterizer,
                            projection,
                            sourceSampler,
                            material);
}

std::vector<RasterSample> projectBrushDabs(std::span<const BrushDab> dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection,
                                           const RasterSourceSampler &sourceSampler,
                                           const BrushMaterial &material)
{
    std::vector<RasterSample> samples;
    samples.reserve(projectedSampleCapacity(dabs, rasterizer));
    ProjectionContext context;
    context.sourceSampler = &sourceSampler;
    context.material = &material;
    context.projection = projection;
    for (const BrushDab &dab : dabs) {
        appendBrushProjection(samples, projectedDab(dab, projection), rasterizer, context);
    }

    return samples;
}

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve, const Rasterizer &rasterizer)
{
    return projectBrushDabs(placeBrushDabs(curve, rasterizer), rasterizer);
}

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve,
                                               const Rasterizer &rasterizer,
                                               const RasterProjection &projection)
{
    return projectBrushDabs(placeBrushDabs(curve, rasterizer), rasterizer, projection);
}

DocumentRect documentBoundsForBrushDab(const BrushDab &dab, const Rasterizer &rasterizer)
{
    return documentRectFromBounds(boundsForDab(dab, rasterizer));
}

DocumentRect documentBoundsForBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer)
{
    DocumentRect bounds{};
    for (const BrushDab &dab : dabs) {
        bounds = uniteDocumentRects(bounds, documentBoundsForBrushDab(dab, rasterizer));
    }

    return bounds;
}

std::vector<DocumentRect> documentBoundsForEachBrushDab(const std::vector<BrushDab> &dabs,
                                                       const Rasterizer &rasterizer)
{
    std::vector<DocumentRect> bounds;
    bounds.reserve(dabs.size());
    for (const BrushDab &dab : dabs) {
        bounds.push_back(documentBoundsForBrushDab(dab, rasterizer));
    }

    return bounds;
}

DevicePixelRect deviceBoundsForBrushDab(const BrushDab &dab,
                                        const Rasterizer &rasterizer,
                                        const RasterProjection &projection)
{
    return deviceRectFromBounds(boundsForDab(projectedDab(dab, projection), rasterizer));
}

std::vector<DevicePixelRect> deviceBoundsForBrushDabs(const std::vector<BrushDab> &dabs,
                                                      const Rasterizer &rasterizer,
                                                      const RasterProjection &projection)
{
    return deviceBoundsForBrushDabs(std::span<const BrushDab>{dabs.data(), dabs.size()},
                                    rasterizer,
                                    projection);
}

std::vector<DevicePixelRect> deviceBoundsForBrushDabs(std::span<const BrushDab> dabs,
                                                      const Rasterizer &rasterizer,
                                                      const RasterProjection &projection)
{
    std::vector<DevicePixelRect> bounds;
    bounds.reserve(dabs.size());
    for (const BrushDab &dab : dabs) {
        bounds.push_back(deviceBoundsForBrushDab(dab, rasterizer, projection));
    }

    return bounds;
}
