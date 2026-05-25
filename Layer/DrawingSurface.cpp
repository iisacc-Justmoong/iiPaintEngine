//
// Created by Justmoong on 2026 May 24.
//

#include "DrawingSurface.h"

#include <algorithm>
#include <cstddef>

namespace {

bool contains(const DrawingSurface &surface, DevicePixelPoint position)
{
    return position.x >= 0
            && position.y >= 0
            && position.x < surface.width
            && position.y < surface.height;
}

std::size_t pixelIndex(const DrawingSurface &surface, DevicePixelPoint position)
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(surface.width)
            + static_cast<std::size_t>(position.x);
}

} // namespace

DrawingSurface makeDrawingSurface(Types::Pixel width, Types::Pixel height, std::uint32_t clearArgb)
{
    DrawingSurface surface;
    surface.width = std::max<Types::Pixel>(0, width);
    surface.height = std::max<Types::Pixel>(0, height);
    surface.pixels.assign(static_cast<std::size_t>(surface.width) * static_cast<std::size_t>(surface.height),
                          clearArgb);
    surface.dirtyBounds = {{0, 0}, surface.width, surface.height};
    if (surface.width > 0 && surface.height > 0) {
        surface.dirtyRegion.push_back(surface.dirtyBounds);
    }
    return surface;
}

DrawingSurface drawingSurfaceFromRasterLayer(const RasterLayer &layer)
{
    DrawingSurface surface = makeDrawingSurface(layer.width, layer.height);
    surface.pixels = layer.pixels;
    return surface;
}

RasterLayer rasterLayerFromDrawingSurface(const DrawingSurface &surface)
{
    RasterLayer layer;
    layer.width = std::max<Types::Pixel>(0, surface.width);
    layer.height = std::max<Types::Pixel>(0, surface.height);
    layer.pixels = surface.pixels;
    return layer;
}

std::uint32_t drawingSurfacePixelAt(const DrawingSurface &surface, DevicePixelPoint position)
{
    if (!contains(surface, position)) {
        return 0x00000000U;
    }

    const std::size_t index = pixelIndex(surface, position);
    if (index >= surface.pixels.size()) {
        return 0x00000000U;
    }

    return surface.pixels[index];
}
