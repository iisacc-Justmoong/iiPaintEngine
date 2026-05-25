#include <type_traits>

#include "Canvas/Canvas.h"
#include "Document/PaintDocument.h"
#include "Layer/DrawingSurface.h"
#include "Layer/LayerMetadata.h"
#include "Layer/RasterLayer.h"

namespace {

template <typename, typename = void>
struct HasLayerContainer : std::false_type {
};

template <typename T>
struct HasLayerContainer<T, std::void_t<decltype(std::declval<T>().layers)>> : std::true_type {
};

} // namespace

int main()
{
    static_assert(!HasLayerContainer<DrawingSurface>::value);

    RasterLayer rasterLayer = makeRasterLayer(4, 3);
    paintRasterSamples(rasterLayer, {RasterSample{{2, 1}, 0xFF112233U}});

    PaintDocument document = makePaintDocument(rasterLayer);
    if (document.canvases.size() != 1
            || document.metadata.version != 1) {
        return 1;
    }

    const Canvas &canvas = document.canvases.front();
    if (canvas.surface.width != 4
            || canvas.surface.height != 3
            || canvas.surface.pixelFormat != SurfacePixelFormat::Argb32
            || canvas.surface.colorSpace != SurfaceColorSpace::Srgb
            || canvas.surface.backingStore != SurfaceBackingStore::CpuMemory
            || canvas.layers.layers.size() != 1
            || !canvas.strokes.strokes.empty()) {
        return 1;
    }

    const Layer &baseLayer = canvas.layers.layers.front();
    if (drawingSurfacePixelAt(baseLayer.surface, {2, 1}) != 0xFF112233U
            || baseLayer.metadata.name != "Base"
            || !baseLayer.metadata.visible
            || baseLayer.metadata.opacity != 1.0
            || baseLayer.metadata.blendMode != RasterBlendMode::SourceOver) {
        return 1;
    }

    LayerMetadata overlayMetadata{};
    overlayMetadata.id.bytes[0] = 7;
    overlayMetadata.name = "Overlay";
    overlayMetadata.visible = false;
    overlayMetadata.opacity = 0.5;
    overlayMetadata.blendMode = RasterBlendMode::SourceOver;

    Layer overlayLayer{};
    overlayLayer.metadata = overlayMetadata;
    Canvas layeredCanvas = makeCanvas(makeDrawingSurface(8, 6));
    layeredCanvas.layers.layers.push_back(overlayLayer);
    if (layeredCanvas.layers.layers.front().metadata.id.bytes[0] != 7
            || layeredCanvas.layers.layers.front().metadata.name != "Overlay"
            || layeredCanvas.layers.layers.front().metadata.visible
            || layeredCanvas.layers.layers.front().metadata.opacity != 0.5
            || layeredCanvas.layers.layers.front().metadata.blendMode != RasterBlendMode::SourceOver) {
        return 1;
    }

    CanvasMetadata metadata{};
    metadata.title = "Canvas";
    metadata.author = "Painter";
    metadata.backgroundColor = 0xFFFFFFFFU;
    metadata.intendedExportWidth = 4000;
    metadata.intendedExportHeight = 3000;
    metadata.appVersion = "test";

    Canvas exportCanvas = makeCanvas(makeDrawingSurface(10, 8), metadata);
    if (exportCanvas.metadata.title != "Canvas"
            || exportCanvas.metadata.backgroundColor != 0xFFFFFFFFU
            || exportCanvas.metadata.intendedExportWidth != 4000
            || exportCanvas.surface.width != 10
            || exportCanvas.layers.layers.size() != 0) {
        return 1;
    }

    return 0;
}
