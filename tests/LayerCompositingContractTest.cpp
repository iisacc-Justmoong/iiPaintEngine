#include "Core/RasterBlendMode.h"
#include "Layer/DrawingSurface.h"
#include "Layer/Layer.h"
#include "Layer/LayerStack.h"
#include "Render/Compositor.h"

namespace {

Layer makeSolidLayer(Types::Pixel width, Types::Pixel height, std::uint32_t argb)
{
    Layer layer;
    layer.surface = makeDrawingSurface(width, height, argb);
    return layer;
}

std::uint32_t pixelAt(const RasterLayer &layer, Types::Pixel x, Types::Pixel y)
{
    return rasterLayerPixelAt(layer, {x, y});
}

} // namespace

int main()
{
    static_assert(RasterBlendMode::SourceOver != RasterBlendMode::Multiply);
    static_assert(RasterBlendMode::Multiply != RasterBlendMode::Screen);
    static_assert(RasterBlendMode::Screen != RasterBlendMode::Overlay);

    LayerStack multiplyStack;
    multiplyStack.layers.push_back(makeSolidLayer(1, 1, 0xFF0000FFU));
    Layer multiplyLayer = makeSolidLayer(1, 1, 0xFFFF0000U);
    multiplyLayer.metadata.blendMode = RasterBlendMode::Multiply;
    multiplyStack.layers.push_back(multiplyLayer);
    if (pixelAt(compositeLayerStack(multiplyStack, 1, 1), 0, 0) != 0xFF000000U) {
        return 1;
    }

    LayerStack screenStack;
    screenStack.layers.push_back(makeSolidLayer(1, 1, 0xFF0000FFU));
    Layer screenLayer = makeSolidLayer(1, 1, 0xFFFF0000U);
    screenLayer.metadata.blendMode = RasterBlendMode::Screen;
    screenStack.layers.push_back(screenLayer);
    if (pixelAt(compositeLayerStack(screenStack, 1, 1), 0, 0) != 0xFFFF00FFU) {
        return 1;
    }

    LayerStack overlayStack;
    overlayStack.layers.push_back(makeSolidLayer(1, 1, 0xFF202020U));
    Layer overlayLayer = makeSolidLayer(1, 1, 0xFFFFFFFFU);
    overlayLayer.metadata.blendMode = RasterBlendMode::Overlay;
    overlayStack.layers.push_back(overlayLayer);
    if (pixelAt(compositeLayerStack(overlayStack, 1, 1), 0, 0) != 0xFF404040U) {
        return 1;
    }

    LayerStack maskStack;
    maskStack.layers.push_back(makeSolidLayer(2, 1, 0xFF0000FFU));
    Layer maskedLayer = makeSolidLayer(2, 1, 0xFFFF0000U);
    maskedLayer.mask.enabled = true;
    maskedLayer.mask.width = 2;
    maskedLayer.mask.height = 1;
    maskedLayer.mask.alpha = {0x00, 0xFF};
    maskStack.layers.push_back(maskedLayer);
    const RasterLayer maskedComposite = compositeLayerStack(maskStack, 2, 1);
    if (pixelAt(maskedComposite, 0, 0) != 0xFF0000FFU
            || pixelAt(maskedComposite, 1, 0) != 0xFFFF0000U) {
        return 1;
    }

    LayerStack clippingStack;
    Layer clippingBase = makeSolidLayer(2, 1, 0x00000000U);
    clippingBase.surface.pixels[1] = 0xFF00FF00U;
    clippingStack.layers.push_back(clippingBase);
    Layer clippedLayer = makeSolidLayer(2, 1, 0xFFFF0000U);
    clippedLayer.metadata.clipsToBelow = true;
    clippingStack.layers.push_back(clippedLayer);
    const RasterLayer clippedComposite = compositeLayerStack(clippingStack, 2, 1);
    if (pixelAt(clippedComposite, 0, 0) != 0x00000000U
            || pixelAt(clippedComposite, 1, 0) != 0xFFFF0000U) {
        return 1;
    }

    Layer groupLayer;
    groupLayer.metadata.kind = LayerKind::Group;
    groupLayer.children.push_back(makeSolidLayer(1, 1, 0xFF00FF00U));
    LayerStack groupStack;
    groupStack.layers.push_back(groupLayer);
    if (pixelAt(compositeLayerStack(groupStack, 1, 1), 0, 0) != 0xFF00FF00U) {
        return 1;
    }

    Layer adjustmentLayer;
    adjustmentLayer.metadata.kind = LayerKind::Adjustment;
    adjustmentLayer.metadata.alphaLock = true;
    if (adjustmentLayer.metadata.kind != LayerKind::Adjustment
            || !adjustmentLayer.metadata.alphaLock) {
        return 1;
    }

    return 0;
}
