//
// Created by Justmoong on 2026 May 24.
//

#include "RasterLayer.h"

#include <algorithm>
#include <cstddef>

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

std::uint32_t withAlpha(std::uint32_t argb, std::uint8_t alpha)
{
    return (argb & 0x00FFFFFFU) | ((alpha & 0xFFU) << 24U);
}

bool contains(const RasterLayer &layer, DevicePixelPoint position)
{
    return position.x >= 0
            && position.y >= 0
            && position.x < layer.width
            && position.y < layer.height;
}

std::size_t pixelIndex(const RasterLayer &layer, DevicePixelPoint position)
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(layer.width)
            + static_cast<std::size_t>(position.x);
}

} // namespace

RasterLayer makeRasterLayer(Types::Pixel width, Types::Pixel height, std::uint32_t clearArgb)
{
    RasterLayer layer;
    layer.width = std::max<Types::Pixel>(0, width);
    layer.height = std::max<Types::Pixel>(0, height);
    layer.pixels.assign(static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height),
                        clearArgb);
    return layer;
}

void paintRasterSamples(RasterLayer &layer, const std::vector<RasterSample> &samples)
{
    for (const RasterSample &sample : samples) {
        if (contains(layer, sample.position)) {
            const std::size_t index = pixelIndex(layer, sample.position);
            const std::uint8_t existingAlpha = alphaOf(layer.pixels[index]);
            const std::uint8_t sampleAlpha = alphaOf(sample.argb);
            const std::uint8_t targetAlpha = existingAlpha >= sample.opacityCap
                    ? existingAlpha
                    : static_cast<std::uint8_t>(std::min<int>(
                            sample.opacityCap,
                            static_cast<int>(existingAlpha) + static_cast<int>(sampleAlpha)));
            layer.pixels[index] = withAlpha(sample.argb, targetAlpha);
        }
    }
}

std::uint32_t rasterLayerPixelAt(const RasterLayer &layer, DevicePixelPoint position)
{
    if (!contains(layer, position)) {
        return 0x00000000U;
    }

    const std::size_t index = pixelIndex(layer, position);
    if (index >= layer.pixels.size()) {
        return 0x00000000U;
    }

    return layer.pixels[index];
}
