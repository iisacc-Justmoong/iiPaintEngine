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

namespace {

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

std::uint32_t withAlpha(std::uint32_t argb, std::uint8_t alpha)
{
    return (argb & 0x00FFFFFFU) | ((alpha & 0xFFU) << 24U);
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
    return alphaByte(source * mask * clamp01(rasterizer.opacity) * clamp01(opacityCapScale));
}

void appendCircle(std::vector<RasterSample> &samples,
                  Types::Pixel centerX,
                  Types::Pixel centerY,
                  Types::Pixel radius,
                  const BrushDab &dab,
                  const Rasterizer &rasterizer)
{
    const Types::Pixel clampedRadius = std::max<Types::Pixel>(0, radius);
    const Types::Pixel radiusSquared = clampedRadius * clampedRadius;
    const std::uint8_t alpha = projectedAlpha(dab.colorArgb, 1.0, dab.alpha);
    const std::uint8_t cap = opacityCap(dab.colorArgb, 1.0, rasterizer, dab.opacityCapScale);
    if (alpha == 0 || cap == 0) {
        return;
    }
    const std::uint32_t argb = withAlpha(dab.colorArgb, alpha);

    for (Types::Pixel y = centerY - clampedRadius; y <= centerY + clampedRadius; ++y) {
        for (Types::Pixel x = centerX - clampedRadius; x <= centerX + clampedRadius; ++x) {
            const Types::Pixel dx = x - centerX;
            const Types::Pixel dy = y - centerY;
            if (dx * dx + dy * dy <= radiusSquared) {
                samples.push_back(RasterSample{{x, y}, argb, cap});
            }
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
    const Types::Scalar clampedAlpha = clamp01(maskAlpha);
    if (clampedAlpha <= 0.0 || clampedAlpha >= 1.0) {
        return clampedAlpha;
    }

    const Types::Scalar clampedHardness = std::clamp(hardness, 0.01, 1.0);
    return std::pow(clampedAlpha, 1.0 / clampedHardness);
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
    return applyHardness(bilinearBrushAlphaAt(rasterizer, sourceX, sourceY),
                         rasterizer.hardness);
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
                      const Rasterizer &rasterizer)
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

            const std::uint8_t alpha = projectedAlpha(dab.colorArgb, maskAlpha, dab.alpha);
            const std::uint8_t cap = opacityCap(dab.colorArgb, maskAlpha, rasterizer, dab.opacityCapScale);
            if (alpha == 0 || cap == 0) {
                continue;
            }

            samples.push_back(RasterSample{
                    {x, y},
                    withAlpha(dab.colorArgb, alpha),
                    cap,
                    dab.blendMode,
            });
        }
    }
}

void appendBrushProjection(std::vector<RasterSample> &samples,
                           const BrushDab &dab,
                           const Rasterizer &rasterizer)
{
    const Types::Pixel centerX = roundedPixel(dab.position.x);
    const Types::Pixel centerY = roundedPixel(dab.position.y);
    if (hasBrushImage(rasterizer)) {
        appendBrushImage(samples, dab, rasterizer);
    } else {
        const auto radius = static_cast<Types::Pixel>(
                std::max<Types::Scalar>(0.0, std::lround(static_cast<Types::Scalar>(rasterizer.radius) * dab.scale)));
        appendCircle(samples, centerX, centerY, radius, dab, rasterizer);
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
    const Types::Scalar velocityScale = 1.0 + std::max<Types::Scalar>(0.0, sample.velocity) * rasterizer.velocitySpacing;
    const Types::Scalar baseSpacing = rasterizer.brushSize > 0.0
            ? rasterizer.brushSize * std::max<Types::Scalar>(0.01, rasterizer.spacingRatio)
            : rasterizer.spacing;
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

Types::Scalar materialTextureAlpha(const BrushTexture &texture, std::uint32_t sequenceIndex)
{
    if (!texture.enabled || texture.alpha.empty() || texture.width <= 0 || texture.height <= 0) {
        return 1.0;
    }

    const std::size_t index = static_cast<std::size_t>(sequenceIndex) % texture.alpha.size();
    return static_cast<Types::Scalar>(texture.alpha[index]) / 255.0;
}

Types::Scalar materialFlowScale(const BrushMaterial &material, Types::Scalar textureAlpha)
{
    Types::Scalar scale = 1.0;
    if (material.texture.enabled) {
        scale *= 1.0 - std::clamp(material.texture.grainStrength, 0.0, 1.0) * (1.0 - textureAlpha);
    }
    if (material.dualBrush.enabled) {
        scale *= std::clamp(material.dualBrush.scale, 0.0, 1.0);
    }
    scale *= 1.0 - std::clamp(material.simulation.wetness, 0.0, 1.0) * 0.25;
    scale *= 1.0 - std::clamp(material.simulation.smudgeStrength, 0.0, 1.0) * 0.15;
    scale *= 1.0 - std::clamp(material.simulation.mixStrength, 0.0, 1.0) * 0.10;
    return std::clamp(scale, 0.0, 1.0);
}

DocumentPoint scatterPosition(DocumentPoint position,
                              const BrushScatter &scatter,
                              std::uint32_t randomSeed,
                              std::uint32_t sequenceIndex)
{
    if (!scatter.enabled || scatter.radius <= 0.0) {
        return position;
    }

    const Types::Scalar dx = deterministicSigned(randomSeed + 0x51A7U, sequenceIndex) * scatter.radius;
    const Types::Scalar dy = deterministicSigned(randomSeed + 0x8D31U, sequenceIndex) * scatter.radius;
    return {position.x + dx, position.y + dy};
}

Types::Scalar taperFactor(const Rasterizer &rasterizer, Types::Scalar distanceOnCurve, Types::Scalar curveLength)
{
    Types::Scalar factor = 1.0;
    if (rasterizer.warmupDistance > 0.0) {
        const Types::Scalar warmup = std::clamp(distanceOnCurve / rasterizer.warmupDistance, 0.0, 1.0);
        factor = std::min(factor, 0.25 + warmup * 0.75);
    }
    if (rasterizer.taperDistance > 0.0) {
        const Types::Scalar remaining = std::max<Types::Scalar>(0.0, curveLength - distanceOnCurve);
        const Types::Scalar taper = std::clamp(remaining / rasterizer.taperDistance, 0.0, 1.0);
        factor = std::min(factor, 0.25 + taper * 0.75);
    }
    return factor;
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
    const Types::Scalar pressure = clamp01(sample.pressure);
    const Types::Scalar scale = std::max<Types::Scalar>(
            0.01,
            1.0 + (pressure - 1.0) * rasterizer.pressureScale);
    const bool hasTilt = sample.tiltX != 0.0 || sample.tiltY != 0.0;
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
    const Types::Scalar jitter = deterministicSigned(randomSeed, sequenceIndex) * rasterizer.rotationJitter
            + dynamicsResult.rotationJitterRadians;
    const Types::Scalar baseRotation = dynamicsResult.rotationFromTilt
            ? dynamicsResult.rotationRadians
            : (hasTilt ? std::atan2(sample.tiltY, sample.tiltX) : tangentRadians);
    const Types::Scalar textureDirection = dynamicsResult.textureDirectionFromTilt
            ? dynamicsResult.textureDirectionRadians
            : baseRotation;
    const Types::Scalar textureAlpha = materialTextureAlpha(material.texture, sequenceIndex);
    const DocumentPoint position = scatterPosition(sample.position, material.scatter, randomSeed, sequenceIndex);

    return BrushDab{
            position,
            scale * dynamicsResult.sizeScale * (material.dualBrush.enabled ? std::max<Types::Scalar>(0.01, material.dualBrush.scale) : 1.0),
            baseRotation + jitter,
            clamp01(rasterizer.flow) * dynamicsResult.flowScale * taperFactor(rasterizer, distanceOnCurve, curveLength)
                    * materialFlowScale(material, textureAlpha),
            dynamicsResult.opacityScale,
            dynamicsResult.ellipseScaleX,
            dynamicsResult.ellipseScaleY,
            textureDirection,
            clamp01(dynamicsResult.grain + material.texture.grainStrength * (1.0 - textureAlpha)),
            textureAlpha,
            material.dualBrush.enabled,
            rasterizer.argb,
            RasterBlendMode::SourceOver,
            sequenceIndex,
    };
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
    return projectBrushDabs(dabs, rasterizer, RasterProjection{});
}

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs,
                                           const Rasterizer &rasterizer,
                                           const RasterProjection &projection)
{
    std::vector<RasterSample> samples;
    for (const BrushDab &dab : dabs) {
        appendBrushProjection(samples, projectedDab(dab, projection), rasterizer);
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
    std::vector<DevicePixelRect> bounds;
    bounds.reserve(dabs.size());
    for (const BrushDab &dab : dabs) {
        bounds.push_back(deviceBoundsForBrushDab(dab, rasterizer, projection));
    }

    return bounds;
}
