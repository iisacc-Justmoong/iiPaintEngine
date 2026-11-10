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

enum class RenderAccelerationPath {
    ScalarCpu,
    SimdCpu,
    GpuCompute,
};

enum class RenderBufferFormat {
    UInt8,
    UInt16,
    Float16,
    Float32,
};

struct RenderContext {
    ColorSpace sourceColorSpace;
    ColorSpace targetColorSpace;
    RenderBackend preferredBackend = RenderBackend::Cpu;
    bool allowCpuFallback = true;
    bool tileCacheEnabled = false;
    bool brushStampAtlasEnabled = false;
    bool simdEnabled = true;
    bool gpuPathEnabled = true;
    bool linearCompositingEnabled = true;
};

struct RenderExecutionPlan {
    RenderBackend backend = RenderBackend::Cpu;
    RenderAccelerationPath accelerationPath = RenderAccelerationPath::ScalarCpu;
    RenderBufferFormat bufferFormat = RenderBufferFormat::UInt8;
    bool usesTileCache = false;
    bool usesBrushStampAtlas = false;
    bool linearCompositing = false;
    bool wideGamut = false;
    bool hdr = false;
    bool iccManaged = false;
    bool usedCpuFallback = false;
};

struct RenderResult {
    RasterLayer layer;
    ColorSpace colorSpace;
    RenderExecutionPlan executionPlan;
    RenderBackend backend = RenderBackend::Cpu;
    bool usedCpuFallback = false;
    bool colorSpaceMatched = false;
    EngineError error;
};

bool renderColorSpacesMatch(const ColorSpace &source, const ColorSpace &target);

RenderBufferFormat renderBufferFormatForColorSpace(const ColorSpace &colorSpace);

bool renderColorSpaceRequiresLinearCompositing(const ColorSpace &colorSpace);
