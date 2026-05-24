//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/PaintRect.h"

struct CanvasViewport {
    PaintRect visibleRect;
    double zoom = 1.0;
};
