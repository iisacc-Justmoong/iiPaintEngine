//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Input/PointerEvent.h"
#include "Stroke/StrokeInput.h"

struct InputStrokeBuildResult {
    bool strokeCompleted = false;
    StrokeInput stroke;
};

struct InputStrokeBuilder {
    bool active = false;
    std::vector<StrokePoint> points;
};

InputStrokeBuildResult appendPointerEvent(InputStrokeBuilder &builder, const PointerEvent &event);

StrokeInput activeStrokeInput(const InputStrokeBuilder &builder);

void resetInputStrokeBuilder(InputStrokeBuilder &builder);
