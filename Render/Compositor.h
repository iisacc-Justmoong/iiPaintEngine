//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Layer/LayerStack.h"
#include "Layer/RasterLayer.h"

struct Compositor {
};

RasterLayer compositeLayerStack(const LayerStack &layers,
                                Types::Pixel width,
                                Types::Pixel height,
                                std::uint32_t clearArgb = 0x00000000U);
