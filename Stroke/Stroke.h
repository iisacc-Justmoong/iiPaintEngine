//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Core/PaintUuid.h"
#include "Stroke/StrokePoint.h"

struct Stroke {
    PaintUuid id;
    PaintUuid brushId;
    std::vector<StrokePoint> points;
};
