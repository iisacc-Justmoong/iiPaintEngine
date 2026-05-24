//
// Created by Justmoong on 2026 May 24.
//

#include "RasterLayer.h"

#include <algorithm>
#include <cstddef>

namespace {

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
            layer.pixels[pixelIndex(layer, sample.position)] = sample.argb;
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
