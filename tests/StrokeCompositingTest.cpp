#include <cstdint>
#include <vector>

#include "Layer/RasterLayer.h"

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

std::uint8_t redOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 16U) & 0xFFU);
}

std::uint8_t blueOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>(argb & 0xFFU);
}

} // namespace

int main()
{
    StrokeCompositeBuffer buffer = makeStrokeCompositeBuffer(3, 3);
    const std::vector<RasterSample> repeatedRed{
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
    };

    accumulateStrokeSamples(buffer, repeatedRed);
    if (alphaOf(strokeCompositePixelAt(buffer, {1, 1})) != 0x80) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(3, 3, 0xFF0000FFU);
    compositeStrokeBufferOntoLayer(layer, buffer);
    const std::uint32_t composited = rasterLayerPixelAt(layer, {1, 1});
    if (alphaOf(composited) != 0xFF || redOf(composited) < 0x7F || blueOf(composited) > 0x80) {
        return 1;
    }

    RasterLayer directLayer = makeRasterLayer(3, 3, 0xFF0000FFU);
    paintRasterSamples(directLayer, repeatedRed);
    if (rasterLayerPixelAt(directLayer, {1, 1}) != composited) {
        return 1;
    }

    if (rasterLayerPixelAt(layer, {0, 0}) != 0xFF0000FFU) {
        return 1;
    }

    return 0;
}
