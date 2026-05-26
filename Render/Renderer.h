//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Layer/LayerStack.h"
#include "Render/CpuRenderer.h"
#include "Render/GpuRenderer.h"
#include "Render/RenderContext.h"

struct Renderer {
    RenderContext context;
    CpuRenderer cpu;
    GpuRenderer gpu;
};

RenderResult renderLayerStack(const Renderer &renderer,
                              const LayerStack &layers,
                              Types::Pixel width,
                              Types::Pixel height);
