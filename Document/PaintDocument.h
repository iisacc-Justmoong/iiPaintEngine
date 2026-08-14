//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Document/DocumentMetadata.h"
#include "Layer/DrawingSurface.h"
#include "Layer/LayerStack.h"

struct PaintDocument {
    DocumentMetadata metadata;
    DrawingSurface surface;
    LayerStack layers;
};

PaintDocument makePaintDocument(const RasterLayer &baseLayer);
