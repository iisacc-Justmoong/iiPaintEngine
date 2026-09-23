//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "Brush/BrushPreset.h"
#include "Brush/BrushState.h"

struct BrushResolve {
    std::optional<BrushState> brush;
    std::vector<std::string> errors;
};

// Invalid settings never produce a partially usable runtime brush.
std::vector<std::string> validateBrushPreset(const BrushPreset &preset);
BrushResolve resolveBrushPreset(const BrushPreset &preset,
                               std::uint32_t colorArgb = 0xFF000000U,
                               std::uint32_t randomSeed = 0);
