//
// Created by Justmoong on 2026 May 24.
//

#include "GpuRenderer.h"

#include "Render/Compositor.h"
#include "Render/RenderCache.h"

RenderResult renderLayerStackGpu(const GpuRenderer &renderer,
                                 const CpuRenderer &cpuFallback,
                                 const RenderContext &context,
                                 const LayerStack &layers,
                                 Types::Pixel width,
                                 Types::Pixel height)
{
    RenderResult result;
    result.backend = RenderBackend::Gpu;
    result.colorSpace = context.targetColorSpace;
    result.executionPlan = resolveRenderExecutionPlan(context, cpuFallback, renderer);

    if (!renderColorSpacesMatch(context.sourceColorSpace, context.targetColorSpace)) {
        result.error = {EngineErrorCode::UnsupportedColorTransform, "Color transform is not implemented"};
        return result;
    }

    if (!renderer.available) {
        if (!context.allowCpuFallback) {
            result.error = {EngineErrorCode::InvalidState, "GPU renderer is not available"};
            return result;
        }

        result = renderLayerStackCpu(cpuFallback, context, layers, width, height);
        result.usedCpuFallback = result.error.code == EngineErrorCode::None;
        result.executionPlan = resolveRenderExecutionPlan(context, cpuFallback, renderer);
        return result;
    }

    result.layer = compositeLayerStack(layers, width, height);
    result.colorSpaceMatched = true;
    return result;
}
