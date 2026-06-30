#include <cstdint>
#include <vector>

#include "Stroke/Rasterizer.h"

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

std::uint8_t alphaAt(const std::vector<RasterSample> &samples, DevicePixelPoint position)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y) {
            return alphaOf(sample.argb);
        }
    }

    return 0;
}

} // namespace

int main()
{
    Rasterizer circleRasterizer{};
    circleRasterizer.argb = 0xFFFFFFFFU;
    circleRasterizer.radius = 2;
    circleRasterizer.flow = 1.0;
    circleRasterizer.opacity = 1.0;

    BrushDab circleDab{};
    circleDab.position = {10.0, 10.0};
    circleDab.scale = 1.0;
    circleDab.alpha = 1.0;
    circleDab.colorArgb = 0xFFFFFFFFU;

    const std::vector<RasterSample> circleSamples = projectBrushDabs({circleDab}, circleRasterizer);
    const std::uint8_t circleCenterAlpha = alphaAt(circleSamples, {10, 10});
    const std::uint8_t circleEdgeAlpha = alphaAt(circleSamples, {12, 10});
    if (circleCenterAlpha != 255 || circleEdgeAlpha == 0 || circleEdgeAlpha >= circleCenterAlpha) {
        return 1;
    }

    Rasterizer rasterizer{};
    rasterizer.argb = 0xFFFFFFFFU;
    rasterizer.brushWidth = 2;
    rasterizer.brushHeight = 2;
    rasterizer.brushAlpha = {
            255, 0,
            0, 0,
    };
    rasterizer.flow = 1.0;
    rasterizer.opacity = 1.0;
    rasterizer.hardness = 1.0;

    BrushDab dab{};
    dab.position = {10.0, 10.0};
    dab.scale = 2.0;
    dab.rotationRadians = 0.0;
    dab.alpha = 1.0;
    dab.colorArgb = 0xFFFFFFFFU;

    const std::vector<RasterSample> hardSamples = projectBrushDabs({dab}, rasterizer);
    if (alphaAt(hardSamples, {10, 10}) != 64) {
        return 1;
    }

    dab.position = {10.5, 10.0};
    const std::vector<RasterSample> subpixelSamples = projectBrushDabs({dab}, rasterizer);
    if (alphaAt(subpixelSamples, {10, 10}) <= alphaAt(subpixelSamples, {11, 10})) {
        return 1;
    }

    dab.position = {10.0, 10.0};
    dab.rotationRadians = 0.7853981633974483;
    const std::vector<RasterSample> rotatedSamples = projectBrushDabs({dab}, rasterizer);
    if (rotatedSamples.empty() || alphaAt(rotatedSamples, {10, 10}) == 0) {
        return 1;
    }

    rasterizer.hardness = 0.5;
    const std::vector<RasterSample> softSamples = projectBrushDabs({dab}, rasterizer);
    if (alphaAt(softSamples, {10, 10}) >= alphaAt(rotatedSamples, {10, 10})) {
        return 1;
    }

    Rasterizer tinyRasterizer{};
    tinyRasterizer.argb = 0xFFFFFFFFU;
    tinyRasterizer.brushWidth = 1;
    tinyRasterizer.brushHeight = 1;
    tinyRasterizer.brushAlpha = {255};
    tinyRasterizer.flow = 1.0;
    tinyRasterizer.opacity = 1.0;
    tinyRasterizer.hardness = 1.0;

    BrushDab tinyDab{};
    tinyDab.position = {20.5, 20.0};
    tinyDab.scale = 0.5;
    tinyDab.alpha = 1.0;
    tinyDab.colorArgb = 0xFFFFFFFFU;

    const std::vector<RasterSample> tinySamples = projectBrushDabs({tinyDab}, tinyRasterizer);
    if (alphaAt(tinySamples, {20, 20}) == 0 || alphaAt(tinySamples, {21, 20}) == 0) {
        return 1;
    }

    return 0;
}
