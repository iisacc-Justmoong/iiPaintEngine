#include <cstddef>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

#include "Brush/BrushSnapshot.h"
#include "Color/ColorSpace.h"
#include "Document/DocumentSerializer.h"
#include "History/Command.h"
#include "Layer/DrawingSurface.h"
#include "Layer/Layer.h"

namespace {

PaintUuid uuidWithFirstByte(std::uint8_t value)
{
    PaintUuid uuid{};
    uuid.bytes[0] = value;
    return uuid;
}

template <typename Byte>
bool byteVectorEquals(const std::vector<Byte> &bytes, std::initializer_list<unsigned int> expected)
{
    if (bytes.size() != expected.size()) {
        return false;
    }
    std::size_t index = 0;
    for (const unsigned int expectedByte : expected) {
        const unsigned int actual = [&] {
            if constexpr (std::is_same_v<Byte, std::byte>) {
                return std::to_integer<unsigned int>(bytes[index]);
            } else {
                return static_cast<unsigned int>(bytes[index]);
            }
        }();
        if (actual != expectedByte) {
            return false;
        }
        ++index;
    }
    return true;
}

} // namespace

int main()
{
    DrawingSurface surface = makeDrawingSurface(3, 2);
    surface.pixels[4] = 0xFF224466U;

    PaintDocument document;
    document.metadata.title = "RoundTrip";
    document.metadata.author = "Painter";
    document.metadata.documentId = uuidWithFirstByte(1);
    document.metadata.storagePath = "roundtrip.ipe";

    document.metadata.colorSpace = "Display P3";
    document.metadata.backgroundColor = 0xFF010203U;
    document.surface = surface;

    Layer layer;
    layer.surface = surface;
    layer.metadata.id = uuidWithFirstByte(2);
    layer.metadata.name = "Ink";
    layer.metadata.opacity = 0.75;
    layer.metadata.alphaLock = true;
    layer.mask.enabled = true;
    layer.mask.width = 3;
    layer.mask.height = 2;
    layer.mask.alpha = {0xFF, 0x80, 0x00, 0x40, 0xFF, 0x20};
    Layer childLayer;
    childLayer.metadata.kind = LayerKind::Adjustment;
    childLayer.metadata.name = "Tone";
    layer.children.push_back(childLayer);
    document.layers.layers.push_back(layer);

    BrushSnapshot brush;
    brush.brushId = uuidWithFirstByte(3);
    brush.name = "Archive Brush";
    brush.size = 6.0F;
    brush.opacity = 0.9F;
    brush.tip.width = 2;
    brush.tip.height = 2;
    brush.tip.mask = {std::byte{0x00}, std::byte{0x7F}, std::byte{0xCC}, std::byte{0xFF}};
    brush.dynamics.sizeResponse.enabled = true;
    brush.dynamics.sizeResponse.pressure.enabled = true;
    brush.dynamics.sizeResponse.pressure.min = 0.25;
    brush.dynamics.sizeResponse.pressure.center = 0.5;
    brush.dynamics.sizeResponse.pressure.max = 1.0;
    brush.material.texture.enabled = true;
    brush.material.texture.space = BrushTextureSpace::Document;
    brush.material.texture.width = 2;
    brush.material.texture.height = 1;
    brush.material.texture.alpha = {128, 255};
    brush.material.texture.assetCache.enabled = true;
    brush.material.texture.assetCache.assetId = uuidWithFirstByte(8);
    brush.material.texture.assetCache.cacheKey = "document-texture-cache";
    brush.material.texture.assetCache.width = 2;
    brush.material.texture.assetCache.height = 1;
    brush.material.texture.assetCache.alpha = {255, 64};
    brush.material.paperGrain.enabled = true;
    brush.material.paperGrain.space = BrushTextureSpace::Paper;
    brush.material.paperGrain.width = 2;
    brush.material.paperGrain.height = 1;
    brush.material.paperGrain.alpha = {32, 255};
    brush.material.dualBrush.enabled = true;
    brush.material.dualBrush.compositeMode = DualBrushCompositeMode::Difference;
    brush.material.simulation.enabled = true;
    brush.material.simulation.model = BrushSimulationModel::Mixer;
    brush.material.bristle.enabled = true;
    brush.material.bristle.shape = BristleShape::Fan;
    brush.material.bristle.count = 9;

    ColorSpace colorSpace = makeDisplayP3LinearFloatColorSpace();
    colorSpace.iccProfile = {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};

    DocumentAsset asset;
    asset.id = uuidWithFirstByte(4);
    asset.name = "paper";
    asset.mimeType = "image/png";
    asset.bytes = {std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47}};

    Command historyCommand;
    historyCommand.sequence = 1;
    historyCommand.label = "Paint bitmap";
    historyCommand.targetId = layer.metadata.id;
    historyCommand.kind = CommandKind::PaintStroke;
    historyCommand.scope = CommandScope::Layer;
    historyCommand.transactionId = uuidWithFirstByte(5);
    historyCommand.dirtyBounds = {{0.0, 0.0}, 3.0, 2.0};
    CommandPatch patch;
    patch.targetId = layer.metadata.id;
    patch.scope = CommandScope::Layer;
    patch.dirtyBounds = historyCommand.dirtyBounds;
    patch.beforeState.storage = CommandPayloadStorage::InlineBytes;
    patch.beforeState.bytes = {std::byte{0x00}, std::byte{0x11}};
    patch.afterState.storage = CommandPayloadStorage::InlineBytes;
    patch.afterState.bytes = {std::byte{0xAA}, std::byte{0xBB}};
    historyCommand.patches.push_back(patch);

    DocumentArchive archive = makeDocumentArchive(document);
    archive.brushSources.push_back(brush);
    archive.colorSpaces.push_back(colorSpace);
    archive.assets.push_back(asset);
    archive.history.undoCommands.push_back(historyCommand);
    archive.history.cursor = 1;
    archive.history.nextSequence = 2;

    const std::string payload = serializeDocumentArchive(archive);
    if (payload.find("strokes") != std::string::npos
            || payload.find("StrokeCurve") != std::string::npos
            || payload.find("rawInput") != std::string::npos) {
        return 1;
    }
    const DocumentArchive reopened = deserializeDocumentArchive(payload);
    if (reopened.formatMagic != "iiPaintDocument"
            || reopened.formatVersion != 3
            || reopened.document.metadata.title != "RoundTrip"
            || reopened.document.metadata.colorSpace != "Display P3"
            || reopened.document.layers.layers.size() != 1) {
        return 1;
    }

    const Layer &reopenedLayer = reopened.document.layers.layers.front();
    if (reopenedLayer.metadata.id.bytes[0] != 2
            || reopenedLayer.metadata.name != "Ink"
            || reopenedLayer.metadata.opacity != 0.75
            || !reopenedLayer.metadata.alphaLock
            || !reopenedLayer.mask.enabled
            || !byteVectorEquals(reopenedLayer.mask.alpha, {0xFF, 0x80, 0x00, 0x40, 0xFF, 0x20})
            || reopenedLayer.children.size() != 1
            || drawingSurfacePixelAt(reopenedLayer.surface, {1, 1}) != 0xFF224466U) {
        return 1;
    }

    if (reopened.brushSources.size() != 1
            || reopened.brushSources.front().brushId.bytes[0] != 3
            || !byteVectorEquals(reopened.brushSources.front().tip.mask, {0x00, 0x7F, 0xCC, 0xFF})
            || !reopened.brushSources.front().dynamics.sizeResponse.pressure.enabled
            || reopened.brushSources.front().material.texture.space != BrushTextureSpace::Document
            || !byteVectorEquals(reopened.brushSources.front().material.texture.assetCache.alpha, {0xFF, 0x40})
            || !reopened.brushSources.front().material.paperGrain.enabled
            || reopened.brushSources.front().material.dualBrush.compositeMode != DualBrushCompositeMode::Difference
            || reopened.brushSources.front().material.simulation.model != BrushSimulationModel::Mixer
            || reopened.brushSources.front().material.bristle.count != 9
            || reopened.colorSpaces.size() != 1
            || !byteVectorEquals(reopened.colorSpaces.front().iccProfile, {0x10, 0x20, 0x30})
            || reopened.assets.size() != 1
            || !byteVectorEquals(reopened.assets.front().bytes, {0x89, 0x50, 0x4E, 0x47})
            || reopened.history.undoCommands.size() != 1
            || reopened.history.undoCommands.front().patches.size() != 1
            || !byteVectorEquals(reopened.history.undoCommands.front().patches.front().beforeState.bytes,
                                 {0x00, 0x11})
            || !byteVectorEquals(reopened.history.undoCommands.front().patches.front().afterState.bytes,
                                 {0xAA, 0xBB})) {
        return 1;
    }
    const std::string legacyPayload =
            "formatMagic\t\"iiPaintDocument\"\n"
            "formatVersion\t2\n"
            "document.metadata.title\t\"Legacy\"\n"
            "document.canvases.count\t1\n"
            "document.canvases.0.metadata.backgroundColor\t4278256131\n"
            "document.canvases.0.metadata.intendedExportWidth\t99\n"
            "document.canvases.0.metadata.colorSpace\t\"sRGB\"\n"
            "document.canvases.0.surface.width\t2\n"
            "document.canvases.0.surface.height\t1\n"
            "document.canvases.0.surface.pixels\tFF010203FF040506\n"
            "document.canvases.0.layers.count\t1\n"
            "document.canvases.0.layers.activeLayerIndex\t0\n"
            "document.canvases.0.layers.0.surface.width\t2\n"
            "document.canvases.0.layers.0.surface.height\t1\n"
            "document.canvases.0.layers.0.surface.pixels\tFF010203FF040506\n"
            "document.canvases.0.layers.0.metadata.name\t\"Legacy Layer\"\n"
            "document.canvases.0.layers.0.metadata.visible\t1\n"
            "document.canvases.0.layers.0.metadata.opacity\t1\n"
            "document.canvases.0.layers.0.children.count\t0\n";
    const DocumentArchive migrated = deserializeDocumentArchive(legacyPayload);
    if (migrated.formatVersion != documentArchiveFormatVersion
            || migrated.document.metadata.title != "Legacy"
            || migrated.document.metadata.intendedExportWidth != 99
            || migrated.document.surface.width != 2
            || migrated.document.layers.layers.size() != 1
            || migrated.document.layers.layers.front().metadata.name != "Legacy Layer"
            || drawingSurfacePixelAt(migrated.document.layers.layers.front().surface, {1, 0})
                    != 0xFF040506U) {
        return 2;
    }
    const std::string migratedPayload = serializeDocumentArchive(migrated);
    if (migratedPayload.find("formatVersion\t3") == std::string::npos
            || migratedPayload.find("document.canvases.") != std::string::npos
            || migratedPayload.find("document.surface.width\t2") == std::string::npos) {
        return 3;
    }

    std::string unsupportedLegacy = legacyPayload;
    const std::size_t countPosition = unsupportedLegacy.find("document.canvases.count\t1");
    unsupportedLegacy.replace(countPosition,
                              std::string{"document.canvases.count\t1"}.size(),
                              "document.canvases.count\t2");
    const DocumentArchive rejected = deserializeDocumentArchive(unsupportedLegacy);
    if (rejected.compatible
            || rejected.compatibilityError.empty()
            || !serializeDocumentArchive(rejected).empty()) {
        return 4;
    }
    return 0;
}
