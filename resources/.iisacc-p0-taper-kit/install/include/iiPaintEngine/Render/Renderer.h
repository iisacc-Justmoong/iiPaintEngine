//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Layer/LayerStack.h"
#include "Render/CpuRenderer.h"
#include "Render/GpuRenderer.h"
#include "Render/RenderCache.h"
#include "Render/RenderContext.h"

struct Renderer {
    RenderContext context;
    CpuRenderer cpu;
    GpuRenderer gpu;
    RenderTileCache tileCache;
    BrushStampAtlas brushStampAtlas;
};

RenderResult renderLayerStack(const Renderer &renderer,
                              const LayerStack &layers,
                              Types::Pixel width,
                              Types::Pixel height);
