//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <string>

#include "Brush/BrushDynamics.h"
#include "Brush/BrushMaterial.h"
#include "Brush/BrushTip.h"
#include "Core/PaintUuid.h"

struct BrushPreset {
    PaintUuid brushId;
    std::string name;
    BrushTip tip;
    float size = 0.0F;
    float opacity = 0.0F;
    float hardness = 0.0F;
    float flow = 0.0F;
    float density = 0.0F;
    BrushDynamics dynamics;
    BrushMaterial material;
};
