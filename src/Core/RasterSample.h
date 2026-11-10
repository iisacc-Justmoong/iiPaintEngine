//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Core/PaintPoint.h"
#include "Core/RasterBlendMode.h"

struct RasterSample {
    DevicePixelPoint position;
    // Alpha is this deposit's effective contribution to an empty stroke.
    std::uint32_t argb = 0x00000000U;
    // Per-stroke source-opacity limit; tip and material coverage do not cap it.
    std::uint8_t opacityCap = 0xFFU;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
};
