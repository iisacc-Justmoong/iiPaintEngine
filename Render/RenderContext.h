//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Color/ColorSpace.h"
#include "Core/EngineError.h"
#include "Layer/RasterLayer.h"

enum class RenderBackend {
    Cpu,
    Gpu,
};

struct RenderContext {
    ColorSpace sourceColorSpace;
    ColorSpace targetColorSpace;
    RenderBackend preferredBackend = RenderBackend::Cpu;
    bool allowCpuFallback = true;
};

struct RenderResult {
    RasterLayer layer;
    ColorSpace colorSpace;
    RenderBackend backend = RenderBackend::Cpu;
    bool usedCpuFallback = false;
    bool colorSpaceMatched = false;
    EngineError error;
};

bool renderColorSpacesMatch(const ColorSpace &source, const ColorSpace &target);
