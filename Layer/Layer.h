//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Layer/DrawingSurface.h"
#include "Layer/LayerMetadata.h"

struct Layer {
    DrawingSurface surface;
    LayerMetadata metadata;
    LayerMask mask;
    std::vector<Layer> children;
};
