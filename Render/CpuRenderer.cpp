//
// Created by Justmoong on 2026 May 24.
//

#include "CpuRenderer.h"

#include "Render/Compositor.h"

RenderResult renderLayerStackCpu(const CpuRenderer &renderer,
                                 const RenderContext &context,
                                 const LayerStack &layers,
                                 Types::Pixel width,
                                 Types::Pixel height)
{
    RenderResult result;
    result.backend = RenderBackend::Cpu;
    result.colorSpace = context.targetColorSpace;

    if (!renderer.available) {
        result.error = {EngineErrorCode::InvalidState, "CPU renderer is not available"};
        return result;
    }

    if (!renderColorSpacesMatch(context.sourceColorSpace, context.targetColorSpace)) {
        result.error = {EngineErrorCode::UnsupportedColorTransform, "Color transform is not implemented"};
        return result;
    }

    result.layer = compositeLayerStack(layers, width, height);
    result.colorSpaceMatched = true;
    return result;
}
