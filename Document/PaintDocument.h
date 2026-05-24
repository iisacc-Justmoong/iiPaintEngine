//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Layer/RasterLayer.h"

struct PaintDocument {
    std::vector<RasterLayer> rasterLayers;
};

PaintDocument makePaintDocument(const RasterLayer &baseLayer);
