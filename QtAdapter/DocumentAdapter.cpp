//
// Created by Justmoong on 2026 May 24.
//

#include "DocumentAdapter.h"

#include "Document/PaintDocument.h"
#include "History/HistoryStack.h"
#include "Layer/DrawingSurface.h"
#include "Layer/RasterLayer.h"
#include "Render/Compositor.h"

#include <algorithm>

namespace {

Canvas *canvasAt(DocumentAdapter &adapter, std::size_t canvasIndex)
{
    if (canvasIndex >= adapter.archive.document.canvases.size()) {
        return nullptr;
    }
    return &adapter.archive.document.canvases[canvasIndex];
}

const Canvas *canvasAt(const DocumentAdapter &adapter, std::size_t canvasIndex)
{
    if (canvasIndex >= adapter.archive.document.canvases.size()) {
        return nullptr;
    }
    return &adapter.archive.document.canvases[canvasIndex];
}

Types::Scalar clampUnit(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Pixel canvasWidth(const Canvas &canvas)
{
    if (canvas.surface.width > 0) {
        return canvas.surface.width;
    }
    if (!canvas.layers.layers.empty()) {
        return canvas.layers.layers.front().surface.width;
    }
    return 0;
}

Types::Pixel canvasHeight(const Canvas &canvas)
{
    if (canvas.surface.height > 0) {
        return canvas.surface.height;
    }
    if (!canvas.layers.layers.empty()) {
        return canvas.layers.layers.front().surface.height;
    }
    return 0;
}

Command makePaintStrokeHistoryCommand(const Layer &layer, const StrokeCommand &stroke)
{
    Command command;
    command.label = "Paint stroke";
    command.kind = CommandKind::PaintStroke;
    command.scope = CommandScope::Layer;
    command.targetId = layer.metadata.id;
    command.dirtyBounds = stroke.dirtyBounds;

    CommandPatch patch;
    patch.targetId = layer.metadata.id;
    patch.scope = CommandScope::Layer;
    patch.dirtyBounds = stroke.dirtyBounds;
    patch.beforeState.storage = CommandPayloadStorage::None;
    patch.afterState.storage = CommandPayloadStorage::None;
    command.patches.push_back(patch);
    return command;
}

void refreshCanvasSurface(Canvas &canvas)
{
    const Types::Pixel width = canvasWidth(canvas);
    const Types::Pixel height = canvasHeight(canvas);
    const RasterLayer composite = compositeLayerStack(canvas.layers,
                                                      width,
                                                      height);
    canvas.surface = drawingSurfaceFromRasterLayer(composite);
}

} // namespace

DocumentAdapter makeDocumentAdapter(Types::Pixel width, Types::Pixel height, std::uint32_t clearArgb)
{
    DocumentAdapter adapter;
    const RasterLayer baseLayer = makeRasterLayer(width, height, clearArgb);
    adapter.archive = makeDocumentArchive(makePaintDocument(baseLayer));
    adapter.activeCanvasIndex = 0;
    return adapter;
}

bool loadDocumentArchivePayload(DocumentAdapter &adapter, const std::string &payload)
{
    DocumentArchive archive = deserializeDocumentArchive(payload);
    if (archive.document.canvases.empty()) {
        return false;
    }

    adapter.archive = archive;
    adapter.activeCanvasIndex = 0;
    for (Canvas &canvas : adapter.archive.document.canvases) {
        if (canvas.layers.activeLayerIndex >= canvas.layers.layers.size()) {
            canvas.layers.activeLayerIndex = 0;
        }
        if (canvas.surface.width <= 0 || canvas.surface.height <= 0) {
            refreshCanvasSurface(canvas);
        }
    }
    return true;
}

std::string saveDocumentArchivePayload(const DocumentAdapter &adapter)
{
    return serializeDocumentArchive(adapter.archive);
}

bool documentAdapterHasDocument(const DocumentAdapter &adapter)
{
    return activeDocumentCanvas(adapter) != nullptr;
}

std::size_t documentCanvasCount(const DocumentAdapter &adapter)
{
    return adapter.archive.document.canvases.size();
}

std::size_t documentLayerCount(const DocumentAdapter &adapter)
{
    const Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr) {
        return 0;
    }
    return canvas->layers.layers.size();
}

bool selectDocumentCanvas(DocumentAdapter &adapter, std::size_t canvasIndex)
{
    if (canvasAt(adapter, canvasIndex) == nullptr) {
        return false;
    }

    adapter.activeCanvasIndex = canvasIndex;
    return true;
}

bool selectDocumentLayer(DocumentAdapter &adapter, std::size_t layerIndex)
{
    Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr || layerIndex >= canvas->layers.layers.size()) {
        return false;
    }

    canvas->layers.activeLayerIndex = layerIndex;
    return true;
}

Canvas *activeDocumentCanvas(DocumentAdapter &adapter)
{
    return canvasAt(adapter, adapter.activeCanvasIndex);
}

const Canvas *activeDocumentCanvas(const DocumentAdapter &adapter)
{
    return canvasAt(adapter, adapter.activeCanvasIndex);
}

Layer *activeDocumentLayer(DocumentAdapter &adapter)
{
    Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr || canvas->layers.activeLayerIndex >= canvas->layers.layers.size()) {
        return nullptr;
    }
    return &canvas->layers.layers[canvas->layers.activeLayerIndex];
}

const Layer *activeDocumentLayer(const DocumentAdapter &adapter)
{
    const Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr || canvas->layers.activeLayerIndex >= canvas->layers.layers.size()) {
        return nullptr;
    }
    return &canvas->layers.layers[canvas->layers.activeLayerIndex];
}

Layer *documentLayerAt(DocumentAdapter &adapter, std::size_t layerIndex)
{
    Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr || layerIndex >= canvas->layers.layers.size()) {
        return nullptr;
    }
    return &canvas->layers.layers[layerIndex];
}

const Layer *documentLayerAt(const DocumentAdapter &adapter, std::size_t layerIndex)
{
    const Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr || layerIndex >= canvas->layers.layers.size()) {
        return nullptr;
    }
    return &canvas->layers.layers[layerIndex];
}

bool addDocumentRasterLayer(DocumentAdapter &adapter,
                            const std::string &name,
                            std::uint32_t clearArgb,
                            bool selectCreatedLayer)
{
    Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr) {
        return false;
    }

    Layer layer;
    layer.surface = makeDrawingSurface(canvasWidth(*canvas), canvasHeight(*canvas), clearArgb);
    layer.metadata.name = name;
    layer.metadata.id.bytes[0] = static_cast<std::uint8_t>(canvas->layers.layers.size() + 1U);
    canvas->layers.layers.push_back(layer);
    if (selectCreatedLayer) {
        canvas->layers.activeLayerIndex = canvas->layers.layers.size() - 1U;
    }
    refreshActiveDocumentCanvasSurface(adapter);
    return true;
}

bool renameDocumentLayer(DocumentAdapter &adapter, std::size_t layerIndex, const std::string &name)
{
    Layer *layer = documentLayerAt(adapter, layerIndex);
    if (layer == nullptr) {
        return false;
    }

    layer->metadata.name = name;
    return true;
}

bool setDocumentLayerVisible(DocumentAdapter &adapter, std::size_t layerIndex, bool visible)
{
    Layer *layer = documentLayerAt(adapter, layerIndex);
    if (layer == nullptr) {
        return false;
    }

    layer->metadata.visible = visible;
    refreshActiveDocumentCanvasSurface(adapter);
    return true;
}

bool setDocumentLayerOpacity(DocumentAdapter &adapter, std::size_t layerIndex, Types::Scalar opacity)
{
    Layer *layer = documentLayerAt(adapter, layerIndex);
    if (layer == nullptr) {
        return false;
    }

    layer->metadata.opacity = clampUnit(opacity);
    refreshActiveDocumentCanvasSurface(adapter);
    return true;
}

bool commitStrokeToActiveDocumentLayer(DocumentAdapter &adapter,
                                       const StrokeCommand &command,
                                       const std::vector<RasterSample> &samples)
{
    Canvas *canvas = activeDocumentCanvas(adapter);
    Layer *layer = activeDocumentLayer(adapter);
    if (canvas == nullptr || layer == nullptr) {
        return false;
    }

    RasterLayer rasterLayer = rasterLayerFromDrawingSurface(layer->surface);
    paintRasterSamples(rasterLayer, samples);
    layer->surface = drawingSurfaceFromRasterLayer(rasterLayer);
    canvas->strokes.strokes.push_back(command);
    recordHistoryCommand(adapter.archive.history, makePaintStrokeHistoryCommand(*layer, command));
    refreshActiveDocumentCanvasSurface(adapter);
    return true;
}

void refreshActiveDocumentCanvasSurface(DocumentAdapter &adapter)
{
    Canvas *canvas = activeDocumentCanvas(adapter);
    if (canvas == nullptr) {
        return;
    }

    refreshCanvasSurface(*canvas);
}
