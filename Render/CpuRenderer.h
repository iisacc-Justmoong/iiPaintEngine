//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Layer/LayerStack.h"
#include "Render/RenderContext.h"

struct CpuRenderer {
    bool available = true;
};

RenderResult renderLayerStackCpu(const CpuRenderer &renderer,
                                 const RenderContext &context,
                                 const LayerStack &layers,
                                 Types::Pixel width,
                                 Types::Pixel height);
