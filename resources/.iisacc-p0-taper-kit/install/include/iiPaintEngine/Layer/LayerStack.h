//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <vector>

#include "Layer/Layer.h"

struct LayerStack {
    std::vector<Layer> layers;
    std::size_t activeLayerIndex = 0;
};
