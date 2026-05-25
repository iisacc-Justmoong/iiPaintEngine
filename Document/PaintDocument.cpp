//
// Created by Justmoong on 2026 May 24.
//

#include "PaintDocument.h"

PaintDocument makePaintDocument(const RasterLayer &baseLayer)
{
    PaintDocument document;
    const DrawingSurface surface = drawingSurfaceFromRasterLayer(baseLayer);
    Canvas canvas = makeCanvas(surface);
    Layer layer;
    layer.surface = surface;
    layer.name = "Base";
    canvas.layers.layers.push_back(layer);
    document.canvases.push_back(canvas);
    return document;
}
