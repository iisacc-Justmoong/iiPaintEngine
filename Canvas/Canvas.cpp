//
// Created by Justmoong on 2026 May 24.
//

#include "Canvas.h"

Canvas makeCanvas(const DrawingSurface &surface, const CanvasMetadata &metadata)
{
    Canvas canvas;
    canvas.surface = surface;
    canvas.metadata = metadata;
    return canvas;
}
