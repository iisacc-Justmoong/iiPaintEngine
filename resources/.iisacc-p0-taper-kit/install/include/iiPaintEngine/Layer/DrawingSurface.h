//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/PaintRect.h"
#include "Core/Types.h"
#include "Layer/RasterLayer.h"

enum class SurfacePixelFormat {
    Argb32,
};

enum class SurfaceColorSpace {
    Srgb,
};

enum class SurfaceBackingStore {
    CpuMemory,
    GpuTexture,
};

struct DrawingSurface {
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    SurfacePixelFormat pixelFormat = SurfacePixelFormat::Argb32;
    SurfaceColorSpace colorSpace = SurfaceColorSpace::Srgb;
    Types::Scalar dpiX = 72.0;
    Types::Scalar dpiY = 72.0;
    SurfaceBackingStore backingStore = SurfaceBackingStore::CpuMemory;
    std::vector<std::uint32_t> pixels;
    std::vector<DevicePixelRect> dirtyRegion;
    DevicePixelRect dirtyBounds{};
    std::uint64_t textureHandle = 0;
};

DrawingSurface makeDrawingSurface(Types::Pixel width,
                                  Types::Pixel height,
                                  std::uint32_t clearArgb = 0x00000000U);

DrawingSurface drawingSurfaceFromRasterLayer(const RasterLayer &layer);

RasterLayer rasterLayerFromDrawingSurface(const DrawingSurface &surface);

std::uint32_t drawingSurfacePixelAt(const DrawingSurface &surface, DevicePixelPoint position);
