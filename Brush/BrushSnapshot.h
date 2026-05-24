//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Brush/BrushTip.h"
#include "Core/PaintUuid.h"

struct BrushSnapshot {
    PaintUuid brushId;
    BrushTip tip;
    float size = 0.0F;
    float opacity = 0.0F;
    float hardness = 0.0F;
    float flow = 0.0F;
    float density = 0.0F;
};
