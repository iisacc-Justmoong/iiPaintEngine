//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Canvas/CanvasMetadata.h"
#include "Layer/DrawingSurface.h"
#include "Layer/LayerStack.h"
#include "Stroke/StrokeRepository.h"

struct Canvas {
    DrawingSurface surface;
    CanvasMetadata metadata;
    LayerStack layers;
    StrokeRepository strokes;
};

Canvas makeCanvas(const DrawingSurface &surface, const CanvasMetadata &metadata = {});
