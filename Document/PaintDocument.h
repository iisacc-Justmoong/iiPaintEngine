//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Canvas/Canvas.h"
#include "Document/DocumentMetadata.h"

struct PaintDocument {
    DocumentMetadata metadata;
    std::vector<Canvas> canvases;
};

PaintDocument makePaintDocument(const RasterLayer &baseLayer);
