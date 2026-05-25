#include <cmath>
#include <cstdint>
#include <vector>

#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeInput.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

} // namespace

int main()
{
    StrokeInput input{{
            StrokePoint{{0.0, 0.0}, 0.25, 0.0, 0.0, 0.0, 0.0},
            StrokePoint{{3.0, 4.0}, 0.75, 1.0, 0.0, 0.0, 1.0},
    }};

    const StrokeCurve curve = makeStrokeCurve(input);
    if (curve.samples.size() != 2) {
        return 1;
    }

    if (!nearlyEqual(curve.samples[1].velocity, 5.0)
            || !nearlyEqual(curve.samples[1].pressure, 0.75)
            || !nearlyEqual(curve.samples[1].tiltY, 1.0)) {
        return 1;
    }

    Rasterizer rasterizer{};
    rasterizer.argb = 0xFF202020U;
    rasterizer.brushWidth = 1;
    rasterizer.brushHeight = 1;
    rasterizer.brushAlpha = {255};
    rasterizer.spacing = 2.5;
    rasterizer.opacity = 0.3;
    rasterizer.flow = 0.1;
    rasterizer.density = 1.0;
    rasterizer.pressureScale = 1.0;

    const std::vector<BrushDab> dabs = placeBrushDabs(curve, rasterizer);
    if (dabs.size() != 3) {
        return 1;
    }

    if (!nearlyEqual(dabs[0].position.x, 0.0)
            || !nearlyEqual(dabs[1].position.x, 1.5)
            || !nearlyEqual(dabs[1].position.y, 2.0)
            || !nearlyEqual(dabs[2].position.x, 3.0)
            || !nearlyEqual(dabs[2].position.y, 4.0)) {
        return 1;
    }

    if (!(dabs[2].scale > dabs[0].scale)
            || !nearlyEqual(dabs[2].rotationRadians, std::atan2(1.0, 0.0))
            || !nearlyEqual(dabs[2].alpha, 0.1)) {
        return 1;
    }

    const std::vector<RasterSample> samples = projectBrushDabs(dabs, rasterizer);
    if (samples.size() != dabs.size()) {
        return 1;
    }

    if (alphaOf(samples.front().argb) != 26 || samples.front().opacityCap != 77) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(3, 3);
    const std::vector<RasterSample> repeatedFlowSamples{
            RasterSample{{1, 1}, 0x1A202020U, 77},
            RasterSample{{1, 1}, 0x1A202020U, 77},
            RasterSample{{1, 1}, 0x1A202020U, 77},
            RasterSample{{1, 1}, 0x1A202020U, 77},
    };
    paintRasterSamples(layer, repeatedFlowSamples);

    if (alphaOf(rasterLayerPixelAt(layer, {1, 1})) != 77) {
        return 1;
    }

    return 0;
}
