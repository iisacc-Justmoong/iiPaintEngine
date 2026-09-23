//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>

#include "Brush/BrushPreset.h"

struct BrushPresetSerializer {
    std::string formatMagic = "iiPaintBrushPreset";
    std::uint32_t formatVersion = 2;
};

std::string serializeBrushPreset(const BrushPreset &preset);

BrushPreset deserializeBrushPreset(const std::string &payload);

struct BrushPresetReadResult {
    std::optional<BrushPreset> preset;
    std::vector<std::string> errors;
};

// Checked decode; legacy deserializeBrushPreset throws std::invalid_argument on invalid input.
BrushPresetReadResult readBrushPreset(const std::string &payload);
