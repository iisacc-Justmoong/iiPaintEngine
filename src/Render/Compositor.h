//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Layer/LayerStack.h"
#include "Layer/RasterLayer.h"

struct Compositor {
};

// Straight ARGB32, encoded RGB channels, Porter-Duff source-over after blending.
// noiseKey selects a reproducible Dissolve sample; opacity must be in [0,1].
std::uint32_t compositeRasterPixel(std::uint32_t source, std::uint32_t backdrop,
                                 RasterBlendMode mode, double opacity = 1.0,
                                 std::uint32_t noiseKey = 0);

RasterLayer compositeLayerStack(const LayerStack &layers,
                                Types::Pixel width,
                                Types::Pixel height,
                                std::uint32_t clearArgb = 0x00000000U);
