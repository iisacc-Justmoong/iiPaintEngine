//
// Created by Justmoong on 2026 May 24.
//

#include "Rasterizer.h"

#include <algorithm>
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

std::uint8_t projectedAlpha(std::uint32_t argb, Types::Byte maskAlpha, Types::Scalar alpha)
{
    const auto source = static_cast<Types::Scalar>(sourceAlpha(argb));
    const auto mask = static_cast<Types::Scalar>(maskAlpha) / 255.0;
    return alphaByte(source * mask * clamp01(alpha));
}

std::uint8_t opacityCap(std::uint32_t argb, Types::Byte maskAlpha, const Rasterizer &rasterizer)
{
    const auto source = static_cast<Types::Scalar>(sourceAlpha(argb));
    const auto mask = static_cast<Types::Scalar>(maskAlpha) / 255.0;
    return alphaByte(source * mask * clamp01(rasterizer.opacity));
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
    const std::uint8_t alpha = projectedAlpha(dab.colorArgb, 255, dab.alpha);
    const std::uint8_t cap = opacityCap(dab.colorArgb, 255, rasterizer);
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

void appendBrushImage(std::vector<RasterSample> &samples,
                      Types::Pixel centerX,
                      Types::Pixel centerY,
                      const BrushDab &dab,
                      const Rasterizer &rasterizer)
{
    const Types::Scalar centerOffsetX = static_cast<Types::Scalar>(rasterizer.brushWidth / 2);
    const Types::Scalar centerOffsetY = static_cast<Types::Scalar>(rasterizer.brushHeight / 2);
    const Types::Scalar scale = std::max<Types::Scalar>(0.01, dab.scale);
    const Types::Scalar cosTheta = std::cos(dab.rotationRadians);
    const Types::Scalar sinTheta = std::sin(dab.rotationRadians);

    for (Types::Pixel y = 0; y < rasterizer.brushHeight; ++y) {
        for (Types::Pixel x = 0; x < rasterizer.brushWidth; ++x) {
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(rasterizer.brushWidth)
                    + static_cast<std::size_t>(x);
            const Types::Byte maskAlpha = rasterizer.brushAlpha[index];
            if (maskAlpha == 0) {
                continue;
            }

            const std::uint8_t alpha = projectedAlpha(dab.colorArgb, maskAlpha, dab.alpha);
            const std::uint8_t cap = opacityCap(dab.colorArgb, maskAlpha, rasterizer);
            if (alpha == 0 || cap == 0) {
                continue;
            }

            const Types::Scalar localX = (static_cast<Types::Scalar>(x) - centerOffsetX) * scale;
            const Types::Scalar localY = (static_cast<Types::Scalar>(y) - centerOffsetY) * scale;
            const Types::Scalar rotatedX = localX * cosTheta - localY * sinTheta;
            const Types::Scalar rotatedY = localX * sinTheta + localY * cosTheta;
            samples.push_back(RasterSample{
                    {roundedPixel(static_cast<Types::Scalar>(centerX) + rotatedX),
                     roundedPixel(static_cast<Types::Scalar>(centerY) + rotatedY)},
                    withAlpha(dab.colorArgb, alpha),
                    cap,
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
        appendBrushImage(samples, centerX, centerY, dab, rasterizer);
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
    };
}

Types::Scalar effectiveSpacing(const Rasterizer &rasterizer, const StrokePoint &sample)
{
    const Types::Scalar density = std::max<Types::Scalar>(0.01, rasterizer.density);
    const Types::Scalar velocityScale = 1.0 + std::max<Types::Scalar>(0.0, sample.velocity) * rasterizer.velocitySpacing;
    return std::max<Types::Scalar>(0.01, rasterizer.spacing * std::max<Types::Scalar>(0.01, velocityScale) / density);
}

BrushDab makeBrushDab(const StrokePoint &sample,
                      Types::Scalar tangentRadians,
                      const Rasterizer &rasterizer)
{
    const Types::Scalar pressure = clamp01(sample.pressure);
    const Types::Scalar scale = std::max<Types::Scalar>(
            0.01,
            1.0 + (pressure - 1.0) * rasterizer.pressureScale);
    const bool hasTilt = sample.tiltX != 0.0 || sample.tiltY != 0.0;

    return BrushDab{
            sample.position,
            scale,
            hasTilt ? std::atan2(sample.tiltY, sample.tiltX) : tangentRadians,
            clamp01(rasterizer.flow),
            rasterizer.argb,
            RasterBlendMode::SourceOver,
    };
}

void appendSegmentDabs(std::vector<BrushDab> &dabs,
                       const StrokePoint &start,
                       const StrokePoint &end,
                       const Rasterizer &rasterizer,
                       bool includeStart)
{
    const Types::Scalar dx = end.position.x - start.position.x;
    const Types::Scalar dy = end.position.y - start.position.y;
    const Types::Scalar length = std::hypot(dx, dy);
    const Types::Scalar tangent = std::atan2(dy, dx);

    if (includeStart) {
        dabs.push_back(makeBrushDab(start, tangent, rasterizer));
    }
    if (length == 0.0) {
        return;
    }

    for (Types::Scalar distance = effectiveSpacing(rasterizer, start); distance < length;) {
        const Types::Scalar t = distance / length;
        const StrokePoint sample = interpolateSample(start, end, t);
        dabs.push_back(makeBrushDab(sample, tangent, rasterizer));
        distance += effectiveSpacing(rasterizer, sample);
    }

    dabs.push_back(makeBrushDab(end, tangent, rasterizer));
}

} // namespace

std::vector<BrushDab> placeBrushDabs(const StrokeCurve &curve, const Rasterizer &rasterizer)
{
    std::vector<BrushDab> dabs;
    if (curve.samples.empty()) {
        return dabs;
    }

    if (curve.samples.size() == 1) {
        dabs.push_back(makeBrushDab(curve.samples.front(), 0.0, rasterizer));
        return dabs;
    }

    for (std::size_t index = 0; index + 1 < curve.samples.size(); ++index) {
        appendSegmentDabs(dabs, curve.samples[index], curve.samples[index + 1], rasterizer, index == 0);
    }

    return dabs;
}

std::vector<RasterSample> projectBrushDabs(const std::vector<BrushDab> &dabs, const Rasterizer &rasterizer)
{
    std::vector<RasterSample> samples;
    for (const BrushDab &dab : dabs) {
        appendBrushProjection(samples, dab, rasterizer);
    }

    return samples;
}

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve, const Rasterizer &rasterizer)
{
    return projectBrushDabs(placeBrushDabs(curve, rasterizer), rasterizer);
}
