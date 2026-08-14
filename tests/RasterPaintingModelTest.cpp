#include <cmath>
#include <cstdint>
#include <vector>

#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

const RasterSample *sampleAt(const std::vector<RasterSample> &samples, DevicePixelPoint position)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y) {
            return &sample;
        }
    }
    return nullptr;
}

} // namespace

int main()
{
    BrushState brush{};
    brush.rasterizer.argb = 0xFF202020U;
    brush.rasterizer.brushWidth = 1;
    brush.rasterizer.brushHeight = 1;
    brush.rasterizer.brushAlpha = {255};
    brush.rasterizer.spacing = 2.5;
    brush.rasterizer.opacity = 0.3;
    brush.rasterizer.flow = 0.1;
    brush.rasterizer.density = 1.0;
    brush.rasterizer.pressureScale = 1.0;

    RasterDabStream stream{};
    std::vector<BrushDab> dabs = appendRasterDabs(
            stream,
            StrokePoint{{0.0, 0.0}, 0.25, 0.0, 0.0, 0.0, 0.0},
            brush);
    std::vector<BrushDab> finalDabs = appendRasterDabs(
            stream,
            StrokePoint{{3.0, 4.0}, 0.75, 1.0, 0.0, 0.0, 1.0},
            brush,
            true);
    dabs.insert(dabs.end(), finalDabs.begin(), finalDabs.end());

    if (stream.active || dabs.size() != 3) {
        return 1;
    }

    if (!nearlyEqual(dabs[0].position.x, 0.0)
            || !nearlyEqual(dabs[1].position.x, 1.5)
            || !nearlyEqual(dabs[1].position.y, 2.0)
            || !nearlyEqual(dabs[2].position.x, 3.0)
            || !nearlyEqual(dabs[2].position.y, 4.0)
            || !(dabs[2].scale > dabs[0].scale)
            || !nearlyEqual(dabs[2].rotationRadians, std::atan2(1.0, 0.0))
            || !nearlyEqual(dabs[2].alpha, 0.1)) {
        return 1;
    }

    const std::vector<RasterSample> samples = projectBrushDabs(dabs, brush.rasterizer);
    if (samples.empty()) {
        return 1;
    }

    BrushDab fullMaskDab{};
    fullMaskDab.position = {10.0, 10.0};
    fullMaskDab.alpha = brush.rasterizer.flow;
    fullMaskDab.colorArgb = brush.rasterizer.argb;
    const std::vector<RasterSample> fullMaskSamples = projectBrushDabs({fullMaskDab}, brush.rasterizer);
    const RasterSample *centerSample = sampleAt(fullMaskSamples, {10, 10});
    if (centerSample == nullptr
            || alphaOf(centerSample->argb) != 26
            || centerSample->opacityCap != 77) {
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
    return alphaOf(rasterLayerPixelAt(layer, {1, 1})) == 77 ? 0 : 1;
}
