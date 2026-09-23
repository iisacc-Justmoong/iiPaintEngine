//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>
#include "Brush/BrushPreset.h"

struct BrushLibrary {
    std::vector<BrushPreset> presets;
};

// Editable examples, returned by value; no global mutable registry.
std::vector<BrushPreset> builtInBrushPresets();
