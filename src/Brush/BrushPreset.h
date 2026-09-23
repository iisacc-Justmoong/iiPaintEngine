//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <string>

#include "Brush/BrushDynamics.h"
#include "Brush/BrushMaterial.h"
#include "Brush/BrushTip.h"
#include "Brush/BrushShape.h"
#include "Brush/BrushSettings.h"
#include "Core/PaintUuid.h"

struct BrushPreset {
    PaintUuid brushId;
    std::string name;
    BrushTip tip;
    float size = 16.0F;
    float opacity = 1.0F;
    float hardness = 1.0F;
    float flow = 1.0F;
    float density = 1.0F;
    BrushDynamics dynamics;
    BrushMaterial material;
    BrushShape shape;
    BrushStrokeSettings stroke;
    BrushTipSequence tipSequence;
    BrushColorSettings color;
};
