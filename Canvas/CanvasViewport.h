//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/PaintRect.h"
#include "Core/Types.h"

struct CanvasViewport {
    CanvasRect canvasRect;
    ViewRect viewRect;
    DevicePixelRect devicePixelRect;
    Types::Scalar zoom = 1.0;
};
