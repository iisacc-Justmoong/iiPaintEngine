//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Input/PointerEvent.h"
#include "Stroke/StrokePoint.h"

struct InputStrokeBuildResult {
    bool pointAvailable = false;
    bool strokeStarted = false;
    bool strokeCompleted = false;
    bool strokeCancelled = false;
    StrokePoint point{};
};

struct InputStrokeBuilder {
    bool active = false;
};

InputStrokeBuildResult appendPointerEvent(InputStrokeBuilder &builder, const PointerEvent &event);

void resetInputStrokeBuilder(InputStrokeBuilder &builder);
