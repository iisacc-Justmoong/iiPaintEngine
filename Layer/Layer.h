//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <string>

#include "Layer/DrawingSurface.h"

struct Layer {
    DrawingSurface surface;
    std::string name;
    bool visible = true;
    double opacity = 1.0;
};
