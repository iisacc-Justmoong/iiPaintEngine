#include "Color/ColorSpace.h"
#include "Core/EngineError.h"
#include "Core/RasterBlendMode.h"
#include "Layer/DrawingSurface.h"
#include "Layer/Layer.h"
#include "Layer/LayerStack.h"
#include "Render/Renderer.h"

namespace {

Layer makeSolidLayer(Types::Pixel width, Types::Pixel height, std::uint32_t argb)
{
    Layer layer;
    layer.surface = makeDrawingSurface(width, height, argb);
    return layer;
}

LayerStack makeScreenBlendStack()
{
    LayerStack stack;
    stack.layers.push_back(makeSolidLayer(1, 1, 0xFF0000FFU));
    Layer top = makeSolidLayer(1, 1, 0xFFFF0000U);
    top.metadata.blendMode = RasterBlendMode::Screen;
    stack.layers.push_back(top);
    return stack;
}

ColorSpace namedColorSpace(const char *name)
{
    ColorSpace colorSpace;
    colorSpace.name = name;
    return colorSpace;
}

} // namespace

int main()
{
    Renderer renderer;
    renderer.context.sourceColorSpace = namedColorSpace("sRGB");
    renderer.context.targetColorSpace = namedColorSpace("sRGB");
    renderer.context.preferredBackend = RenderBackend::Cpu;

    const RenderResult cpuResult = renderLayerStack(renderer, makeScreenBlendStack(), 1, 1);
    if (cpuResult.error.code != EngineErrorCode::None
            || cpuResult.backend != RenderBackend::Cpu
            || cpuResult.usedCpuFallback
            || !cpuResult.colorSpaceMatched
            || cpuResult.colorSpace.name != "sRGB"
            || rasterLayerPixelAt(cpuResult.layer, {0, 0}) != 0xFFFF00FFU) {
        return 1;
    }

    renderer.context.preferredBackend = RenderBackend::Gpu;
    renderer.gpu.available = false;
    renderer.context.allowCpuFallback = true;
    const RenderResult fallbackResult = renderLayerStack(renderer, makeScreenBlendStack(), 1, 1);
    if (fallbackResult.error.code != EngineErrorCode::None
            || fallbackResult.backend != RenderBackend::Cpu
            || !fallbackResult.usedCpuFallback
            || rasterLayerPixelAt(fallbackResult.layer, {0, 0}) != 0xFFFF00FFU) {
        return 1;
    }

    renderer.context.allowCpuFallback = false;
    const RenderResult missingGpuResult = renderLayerStack(renderer, makeScreenBlendStack(), 1, 1);
    if (missingGpuResult.error.code != EngineErrorCode::InvalidState
            || missingGpuResult.backend != RenderBackend::Gpu) {
        return 1;
    }

    renderer.context.preferredBackend = RenderBackend::Cpu;
    renderer.context.allowCpuFallback = true;
    renderer.context.sourceColorSpace = namedColorSpace("sRGB");
    renderer.context.targetColorSpace = namedColorSpace("Display P3");
    const RenderResult incompatibleColorResult = renderLayerStack(renderer, makeScreenBlendStack(), 1, 1);
    if (incompatibleColorResult.error.code != EngineErrorCode::UnsupportedColorTransform
            || incompatibleColorResult.colorSpaceMatched
            || !incompatibleColorResult.layer.pixels.empty()) {
        return 1;
    }

    return 0;
}
