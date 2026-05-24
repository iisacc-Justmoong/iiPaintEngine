//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Core/PaintPoint.h"
#include "Stroke/StrokeInput.h"

struct StrokeCurve {
    std::vector<CanvasPoint> points;
};

StrokeCurve makeStrokeCurve(const StrokeInput &input);
