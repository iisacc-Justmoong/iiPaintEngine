//
// Created by Justmoong on 2026 May 24.
//

#include "Renderer.h"

RenderResult renderLayerStack(const Renderer &renderer,
                              const LayerStack &layers,
                              Types::Pixel width,
                              Types::Pixel height)
{
    if (renderer.context.preferredBackend == RenderBackend::Gpu) {
        return renderLayerStackGpu(renderer.gpu, renderer.cpu, renderer.context, layers, width, height);
    }

    return renderLayerStackCpu(renderer.cpu, renderer.context, layers, width, height);
}
