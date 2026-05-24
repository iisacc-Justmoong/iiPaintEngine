//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/PaintPoint.h"
#include "Core/Types.h"

struct StrokePoint {
    CanvasPoint position;
    Types::Scalar pressure = 1.0;
    Types::Scalar time = 0.0;
};
