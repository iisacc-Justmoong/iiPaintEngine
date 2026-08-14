#include <type_traits>

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

template <typename, typename = void>
struct HasStrokeRepository : std::false_type {
};

template <typename T>
struct HasStrokeRepository<T, std::void_t<decltype(std::declval<T>().strokes)>> : std::true_type {
};

} // namespace

int main()
{
    static_assert(!HasLayerContainer<DrawingSurface>::value);
    static_assert(!HasStrokeRepository<PaintDocument>::value,
                  "A bitmap document must not retain vector-like stroke commands.");

    RasterLayer rasterLayer = makeRasterLayer(4, 3);
    paintRasterSamples(rasterLayer, {RasterSample{{2, 1}, 0xFF112233U}});

    PaintDocument document = makePaintDocument(rasterLayer);
    if (document.metadata.version != 1
            || document.surface.width != 4
            || document.surface.height != 3
            || document.surface.pixelFormat != SurfacePixelFormat::Argb32
            || document.surface.colorSpace != SurfaceColorSpace::Srgb
            || document.surface.backingStore != SurfaceBackingStore::CpuMemory
            || document.layers.layers.size() != 1) {
        return 1;
    }

    const Layer &baseLayer = document.layers.layers.front();
    if (drawingSurfacePixelAt(baseLayer.surface, {2, 1}) != 0xFF112233U
            || baseLayer.metadata.name != "Base"
            || !baseLayer.metadata.visible
            || baseLayer.metadata.opacity != 1.0
            || baseLayer.metadata.blendMode != RasterBlendMode::SourceOver) {
        return 2;
    }

    Layer overlayLayer{};
    overlayLayer.metadata.id.bytes[0] = 7;
    overlayLayer.metadata.name = "Overlay";
    overlayLayer.metadata.visible = false;
    overlayLayer.metadata.opacity = 0.5;
    document.layers.layers.push_back(overlayLayer);
    if (document.layers.layers.back().metadata.id.bytes[0] != 7
            || document.layers.layers.back().metadata.name != "Overlay"
            || document.layers.layers.back().metadata.visible
            || document.layers.layers.back().metadata.opacity != 0.5) {
        return 3;
    }

    document.metadata.title = "Bitmap Document";
    document.metadata.author = "Painter";
    document.metadata.backgroundColor = 0xFFFFFFFFU;
    document.metadata.intendedExportWidth = 4000;
    document.metadata.intendedExportHeight = 3000;
    document.metadata.appVersion = "test";
    if (document.metadata.title != "Bitmap Document"
            || document.metadata.backgroundColor != 0xFFFFFFFFU
            || document.metadata.intendedExportWidth != 4000) {
        return 4;
    }

    return 0;
}
