//
// Created by Justmoong on 2026 May 24.
//

#include "PaintDocument.h"

PaintDocument makePaintDocument(const RasterLayer &baseLayer)
{
    PaintDocument document;
    const DrawingSurface surface = drawingSurfaceFromRasterLayer(baseLayer);
    document.surface = surface;
    Layer layer;
    layer.surface = surface;
    layer.metadata.name = "Base";
    document.layers.layers.push_back(layer);
    return document;
}
