//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <cstdint>
#include <string>

#include "Brush/BrushPreset.h"

struct BrushPresetSerializer {
    std::string formatMagic = "iiPaintBrushPreset";
    std::uint32_t formatVersion = 1;
};

std::string serializeBrushPreset(const BrushPreset &preset);

BrushPreset deserializeBrushPreset(const std::string &payload);
