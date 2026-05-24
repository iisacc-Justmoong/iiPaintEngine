//
// Created by Justmoong on 2026 May 24.
//

#include "PaintDocument.h"

PaintDocument makePaintDocument(const RasterLayer &baseLayer)
{
    PaintDocument document;
    document.rasterLayers.push_back(baseLayer);
    return document;
}
