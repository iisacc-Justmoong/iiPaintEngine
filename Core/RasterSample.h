//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Core/PaintPoint.h"

enum class RasterBlendMode {
    SourceOver,
};

struct RasterSample {
    DevicePixelPoint position;
    std::uint32_t argb = 0x00000000U;
    std::uint8_t opacityCap = 0xFFU;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
};
