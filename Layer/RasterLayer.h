//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <vector>

#include "Core/RasterSample.h"
#include "Core/Types.h"

struct RasterLayer {
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<std::uint32_t> pixels;
};

RasterLayer makeRasterLayer(Types::Pixel width,
                            Types::Pixel height,
                            std::uint32_t clearArgb = 0x00000000U);

void paintRasterSamples(RasterLayer &layer, const std::vector<RasterSample> &samples);

std::uint32_t rasterLayerPixelAt(const RasterLayer &layer, DevicePixelPoint position);
