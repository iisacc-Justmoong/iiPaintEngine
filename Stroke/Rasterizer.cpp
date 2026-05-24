//
// Created by Justmoong on 2026 May 24.
//

#include "Rasterizer.h"

#include <algorithm>
#include <cmath>

namespace {

void appendCircle(std::vector<RasterSample> &samples,
                  Types::Pixel centerX,
                  Types::Pixel centerY,
                  Types::Pixel radius,
                  std::uint32_t argb)
{
    const Types::Pixel clampedRadius = std::max<Types::Pixel>(0, radius);
    const Types::Pixel radiusSquared = clampedRadius * clampedRadius;

    for (Types::Pixel y = centerY - clampedRadius; y <= centerY + clampedRadius; ++y) {
        for (Types::Pixel x = centerX - clampedRadius; x <= centerX + clampedRadius; ++x) {
            const Types::Pixel dx = x - centerX;
            const Types::Pixel dy = y - centerY;
            if (dx * dx + dy * dy <= radiusSquared) {
                samples.push_back(RasterSample{{x, y}, argb});
            }
        }
    }
}

Types::Pixel roundedPixel(Types::Scalar value)
{
    return static_cast<Types::Pixel>(std::lround(value));
}

} // namespace

std::vector<RasterSample> rasterizeStrokeCurve(const StrokeCurve &curve, const Rasterizer &rasterizer)
{
    std::vector<RasterSample> samples;
    if (curve.points.empty()) {
        return samples;
    }

    for (std::size_t pointIndex = 0; pointIndex < curve.points.size(); ++pointIndex) {
        const CanvasPoint start = curve.points[pointIndex];
        const CanvasPoint end = pointIndex + 1 < curve.points.size() ? curve.points[pointIndex + 1] : start;
        const Types::Scalar dx = end.x - start.x;
        const Types::Scalar dy = end.y - start.y;
        const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));

        for (int step = 0; step <= steps; ++step) {
            const Types::Scalar t = static_cast<Types::Scalar>(step) / static_cast<Types::Scalar>(steps);
            const Types::Scalar x = start.x + dx * t;
            const Types::Scalar y = start.y + dy * t;
            appendCircle(samples, roundedPixel(x), roundedPixel(y), rasterizer.radius, rasterizer.argb);
        }
    }

    return samples;
}
