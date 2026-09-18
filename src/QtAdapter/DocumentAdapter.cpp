#include "DocumentAdapter.h"

#include "Document/PaintDocument.h"
#include "History/HistoryStack.h"
#include "Layer/DrawingSurface.h"
#include "Layer/RasterLayer.h"
#include "Render/Compositor.h"

#include <algorithm>
#include <utility>

namespace {

Types::Scalar clampUnit(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Pixel documentWidth(const PaintDocument &document)
{
    if (document.surface.width > 0) {
        return document.surface.width;
    }
    if (!document.layers.layers.empty()) {
        return document.layers.layers.front().surface.width;
    }
    return 0;
}

Types::Pixel documentHeight(const PaintDocument &document)
{
    if (document.surface.height > 0) {
        return document.surface.height;
    }
    if (!document.layers.layers.empty()) {
        return document.layers.layers.front().surface.height;
    }
    return 0;
}

Command makeRasterPaintHistoryCommand(const Layer &layer, DocumentRect dirtyBounds)
{
    Command command;
    command.label = "Paint stroke";
    command.kind = CommandKind::PaintStroke;
    command.scope = CommandScope::Layer;
    command.targetId = layer.metadata.id;
    command.dirtyBounds = dirtyBounds;

    CommandPatch patch;
    patch.targetId = layer.metadata.id;
    patch.scope = CommandScope::Layer;
    patch.dirtyBounds = dirtyBounds;
    patch.beforeState.storage = CommandPayloadStorage::None;
    patch.afterState.storage = CommandPayloadStorage::None;
    command.patches.push_back(patch);
    return command;
}

void refreshSurface(PaintDocument &document)
{
    const RasterLayer composite = compositeLayerStack(document.layers,
                                                      documentWidth(document),
                                                      documentHeight(document));
    document.surface = drawingSurfaceFromRasterLayer(composite);
}

} // namespace

DocumentAdapter makeDocumentAdapter(Types::Pixel width, Types::Pixel height, std::uint32_t clearArgb)
{
    DocumentAdapter adapter;
    adapter.archive = makeDocumentArchive(makePaintDocument(makeRasterLayer(width, height, clearArgb)));
    return adapter;
}

bool loadDocumentArchivePayload(DocumentAdapter &adapter, const std::string &payload)
{
    DocumentArchive archive = deserializeDocumentArchive(payload);
    if (!archive.compatible || archive.document.layers.layers.empty()) {
        return false;
    }

    if (archive.document.layers.activeLayerIndex >= archive.document.layers.layers.size()) {
        archive.document.layers.activeLayerIndex = 0;
    }
    if (archive.document.surface.width <= 0 || archive.document.surface.height <= 0) {
        refreshSurface(archive.document);
    }
    if (archive.document.surface.width <= 0 || archive.document.surface.height <= 0) {
        return false;
    }

    adapter.archive = std::move(archive);
    return true;
}

std::string saveDocumentArchivePayload(const DocumentAdapter &adapter)
{
    return serializeDocumentArchive(adapter.archive);
}

bool documentAdapterHasDocument(const DocumentAdapter &adapter)
{
    return !adapter.archive.document.layers.layers.empty()
            && documentWidth(adapter.archive.document) > 0
            && documentHeight(adapter.archive.document) > 0;
}

std::size_t documentLayerCount(const DocumentAdapter &adapter)
{
    return adapter.archive.document.layers.layers.size();
}

bool selectDocumentLayer(DocumentAdapter &adapter, std::size_t layerIndex)
{
    if (layerIndex >= adapter.archive.document.layers.layers.size()) {
        return false;
    }
    adapter.archive.document.layers.activeLayerIndex = layerIndex;
    return true;
}

Layer *activeDocumentLayer(DocumentAdapter &adapter)
{
    LayerStack &layers = adapter.archive.document.layers;
    if (layers.activeLayerIndex >= layers.layers.size()) {
        return nullptr;
    }
    return &layers.layers[layers.activeLayerIndex];
}

const Layer *activeDocumentLayer(const DocumentAdapter &adapter)
{
    const LayerStack &layers = adapter.archive.document.layers;
    if (layers.activeLayerIndex >= layers.layers.size()) {
        return nullptr;
    }
    return &layers.layers[layers.activeLayerIndex];
}

Layer *documentLayerAt(DocumentAdapter &adapter, std::size_t layerIndex)
{
    if (layerIndex >= adapter.archive.document.layers.layers.size()) {
        return nullptr;
    }
    return &adapter.archive.document.layers.layers[layerIndex];
}

const Layer *documentLayerAt(const DocumentAdapter &adapter, std::size_t layerIndex)
{
    if (layerIndex >= adapter.archive.document.layers.layers.size()) {
        return nullptr;
    }
    return &adapter.archive.document.layers.layers[layerIndex];
}

bool addDocumentRasterLayer(DocumentAdapter &adapter,
                            const std::string &name,
                            std::uint32_t clearArgb,
                            bool selectCreatedLayer)
{
    PaintDocument &document = adapter.archive.document;
    const Types::Pixel width = documentWidth(document);
    const Types::Pixel height = documentHeight(document);
    if (width <= 0 || height <= 0) {
        return false;
    }

    Layer layer;
    layer.surface = makeDrawingSurface(width, height, clearArgb);
    layer.metadata.name = name;
    layer.metadata.id.bytes[0] = static_cast<std::uint8_t>(document.layers.layers.size() + 1U);
    document.layers.layers.push_back(layer);
    if (selectCreatedLayer) {
        document.layers.activeLayerIndex = document.layers.layers.size() - 1U;
    }
    refreshDocumentSurface(adapter);
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
    refreshDocumentSurface(adapter);
    return true;
}

bool setDocumentLayerOpacity(DocumentAdapter &adapter, std::size_t layerIndex, Types::Scalar opacity)
{
    Layer *layer = documentLayerAt(adapter, layerIndex);
    if (layer == nullptr) {
        return false;
    }
    layer->metadata.opacity = clampUnit(opacity);
    refreshDocumentSurface(adapter);
    return true;
}

bool commitRasterSamplesToActiveDocumentLayer(DocumentAdapter &adapter,
                                              const std::vector<RasterSample> &samples,
                                              DocumentRect dirtyBounds)
{
    Layer *layer = activeDocumentLayer(adapter);
    if (layer == nullptr) {
        return false;
    }

    RasterLayer rasterLayer = rasterLayerFromDrawingSurface(layer->surface);
    paintRasterSamples(rasterLayer, samples);
    layer->surface = drawingSurfaceFromRasterLayer(rasterLayer);
    recordHistoryCommand(adapter.archive.history, makeRasterPaintHistoryCommand(*layer, dirtyBounds));
    refreshDocumentSurface(adapter);
    return true;
}

void refreshDocumentSurface(DocumentAdapter &adapter)
{
    if (!adapter.archive.document.layers.layers.empty()) {
        refreshSurface(adapter.archive.document);
    }
}
