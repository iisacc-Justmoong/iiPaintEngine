//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Layer/LayerStack.h"
#include "Render/CpuRenderer.h"
#include "Render/RenderContext.h"

struct GpuRenderer {
    bool available = false;
    std::uint64_t deviceHandle = 0;
};

RenderResult renderLayerStackGpu(const GpuRenderer &renderer,
                                 const CpuRenderer &cpuFallback,
                                 const RenderContext &context,
                                 const LayerStack &layers,
                                 Types::Pixel width,
                                 Types::Pixel height);
