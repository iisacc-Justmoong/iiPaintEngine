#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <iiPaintEngine>

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

// Measure the parser alone and require a failure result with a specific diagnostic and no partial archive.
bool rejectsPromptly(const std::string &payload,
                     std::string_view expectedError,
                     std::optional<DocumentArchiveReadLimits> limits = std::nullopt)
{
    const auto begin = std::chrono::steady_clock::now();
    const auto archive = limits ? deserializeDocumentArchiveWithLimits(payload, *limits)
                                : deserializeDocumentArchive(payload);
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    if (archive.compatible || !archive.compatibilityError.contains(expectedError)
            || !archive.document.surface.pixels.empty() || !archive.document.layers.layers.empty()
            || !archive.brushSources.empty()
            || !archive.colorSpaces.empty() || !archive.assets.empty()
            || !archive.history.undoCommands.empty() || !archive.history.redoCommands.empty()
            || elapsed >= std::chrono::milliseconds(50)) {
        std::cerr << "Expected archive error containing '" << expectedError << "', got '"
                  << archive.compatibilityError << "' in "
                  << std::chrono::duration<double, std::milli>(elapsed).count() << " ms\n";
        return false;
    }
    return true;
}

// Build a single chain with a serialized surface field at every level so depth is the only violation.
std::string layerChainPayload(std::size_t depth)
{
    std::string payload = "formatVersion\t3\ndocument.layers.count\t1\n";
    std::string prefix = "document.layers.0";
    for (std::size_t level = 1; level <= depth; ++level) {
        payload += prefix + ".surface.width\t0\n";
        payload += prefix + (level < depth ? ".children.count\t1\n" : ".children.count\t0\n");
        prefix += ".children.0";
    }
    return payload;
}

// 실제 바이트 순서, 문자열 개행, 이동 소유권과 청크 경계 오류를 확인한다.
bool checkBinaryFraming()
{
    constexpr std::string_view signature = "iiPaintDocumentBinary\n";
    PaintDocument document;
    document.surface = makeDrawingSurface(2, 1);
    document.surface.pixels = {0x81020304U, 0xFF090A00U};
    document.metadata.title = "Notes\r\n@document.surface.pixels\t0\nliteral \\n";
    const auto *pixels = document.surface.pixels.data();
    auto archive = makeDocumentArchive(std::move(document));
    if (archive.document.surface.pixels.data() != pixels) return false;

    constexpr std::string_view embedded = "formatVersion\t2\n@document.layers.count\t999999\n";
    const auto embeddedBytes = std::as_bytes(std::span(embedded));
    DocumentAsset asset;
    asset.bytes.assign(embeddedBytes.begin(), embeddedBytes.end());
    archive.assets.push_back(std::move(asset));

    const auto payload = serializeDocumentArchive(archive);
    auto reopened = deserializeDocumentArchiveView(payload);
    std::string expected = "@document.surface.pixels\t8\n";
    expected.append("\x04\x03\x02\x81\x00\x0A\x09\xFF", 8).push_back('\n');
    if (!payload.starts_with(signature) || !payload.ends_with("formatVersion\t4\n") || !payload.contains(expected)
            || !reopened.compatible || reopened.formatVersion != 4
            || reopened.document.metadata.title != archive.document.metadata.title
            || reopened.document.surface.pixels != archive.document.surface.pixels
            || reopened.assets.size() != 1 || reopened.assets.front().bytes != archive.assets.front().bytes) {
        std::cerr << "Version 4 raster bytes, metadata or framing differ\n";
        return false;
    }

    const auto header = std::string(signature) + "formatVersion\t4\n";
    const std::pair<std::string_view, std::string_view> malformed[] = {
        {"@document.surface.pixels\t-1\n", "Invalid archive chunk length"},
        {"@document.surface.pixels\t18446744073709551616\n", "Invalid archive chunk length"},
        {"@document.surface.pixels\t4\nABC", "Truncated archive chunk"},
        {"@document.surface.pixels\t4\nABCD!", "missing delimiter"},
        {"@document.surface.pixels\t3\nABC\n", "Invalid ARGB32 chunk length"},
        {"@document.surface.pixels\n", "Missing archive chunk length"},
        {"@\t0\n\n", "Missing archive chunk name"},
        {"document.surface.pixels\tABCDABCD\n", "Invalid archive field storage"},
        {"@document.layers.count\t1\n1\n", "Invalid archive field storage"},
    };
    for (const auto &[record, error] : malformed) {
        if (!rejectsPromptly(header + std::string(record), error)) return false;
    }
    return rejectsPromptly("formatVersion\t4\n", "binary framing")
            && rejectsPromptly(std::string(signature) + "formatVersion\t3\n", "binary framing")
            && rejectsPromptly(payload.substr(0, payload.rfind("formatVersion\t4\n")), "binary framing");
}

// 손상된 16진수와 잘린 값은 거부하고, 대문자와 소문자가 섞인 정상 값은 그대로 읽는다.
bool checkHexadecimalIntegrity()
{
    const std::string header = "formatVersion\t3\n";
    const std::pair<std::string_view, std::string_view> malformed[] = {
        {"document.surface.width\t1\ndocument.surface.height\t1\ndocument.surface.pixels\tFF11Z233\n",
         "Invalid hexadecimal digit for document.surface.pixels"},
        {"colorSpaces.count\t1\ncolorSpaces.0.name\t\"sRGB\"\ncolorSpaces.0.iccProfile\tFg\n",
         "Invalid hexadecimal digit for colorSpaces.0.iccProfile"},
        {"brushSources.count\t1\nbrushSources.0.presetV2\t0g\n",
         "Invalid embedded brush preset bytes for brushSources.0.presetV2"},
        {"document.metadata.thumbnail\t012345678\n", "Invalid hexadecimal length for document.metadata.thumbnail"},
        {"document.metadata.documentId\t0123456789abcdef0123456789ABCDEG\n",
         "Invalid hexadecimal digit for document.metadata.documentId"},
        {"document.metadata.documentId\t00\n", "Invalid UUID length for document.metadata.documentId"},
        {"document.metadata.documentId\t0123456789ABCDEF0123456789ABCDEF0\n",
         "Invalid UUID length for document.metadata.documentId"},
    };
    for (const auto &[record, error] : malformed) {
        if (!rejectsPromptly(header + std::string(record), error)) return false;
    }

    const auto valid = deserializeDocumentArchive(header
            + "document.surface.width\t1\ndocument.surface.height\t1\ndocument.surface.pixels\taA1122Ff\n"
              "document.metadata.documentId\tAa1234567890aBcDEF001234567890Ab\n");
    return valid.compatible && valid.document.surface.pixels.size() == 1
            && valid.document.surface.pixels.front() == 0xAA1122FFU
            && valid.document.metadata.documentId.bytes.front() == 0xAA
            && valid.document.metadata.documentId.bytes.back() == 0xAB;
}

// 이전 16진수 표면과 바이너리 레이어에서 치수와 실제 픽셀 수가 다른 문서를 거부한다.
bool checkSurfacePixelCounts()
{
    const std::string legacyHeader = "formatVersion\t3\n";
    const std::pair<std::string_view, std::string_view> malformed[] = {
        {"document.surface.width\t2\ndocument.surface.height\t1\ndocument.surface.pixels\tFF112233\n",
         "Surface pixel count mismatch for document.surface.pixels"},
        {"document.surface.width\t1\ndocument.surface.height\t1\ndocument.surface.pixels\tFF112233FF445566\n",
         "Surface pixel count mismatch for document.surface.pixels"},
        {"document.surface.width\t1\ndocument.surface.height\t1\n",
         "Surface pixel count mismatch for document.surface.pixels"},
        {"document.surface.width\t-1\n", "Invalid unsigned decimal for document.surface.width"},
        {"document.surface.height\t2147483648\n", "Size exceeds archive limit for document.surface.height"},
        {"document.surface.width\t65536\ndocument.surface.height\t65536\n", "expected 4294967296"},
    };
    for (const auto &[record, error] : malformed) {
        if (!rejectsPromptly(legacyHeader + std::string(record), error)) return false;
    }
    const std::string binaryHeader = "iiPaintDocumentBinary\nformatVersion\t4\n";
    if (!rejectsPromptly(binaryHeader
            + "document.layers.count\t1\ndocument.layers.0.surface.width\t1\n"
              "document.layers.0.surface.height\t2\n@document.layers.0.surface.pixels\t4\nABCD\n",
            "Surface pixel count mismatch for document.layers.0.surface.pixels")) {
        return false;
    }

    const auto empty = deserializeDocumentArchive(binaryHeader
            + "document.surface.width\t8\ndocument.surface.height\t0\n@document.surface.pixels\t0\n\n");
    return empty.compatible && empty.document.surface.width == 8 && empty.document.surface.height == 0
            && empty.document.surface.pixels.empty();
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
    historyCommand.sequence = 2;
    archive.history.redoCommands.push_back(historyCommand);
    archive.history.cursor = 1;
    archive.history.nextSequence = 3;

    const std::string payload = serializeDocumentArchive(archive);
    if (payload.find("strokes") != std::string::npos
            || payload.find("StrokeCurve") != std::string::npos
            || payload.find("rawInput") != std::string::npos) {
        return 1;
    }
    const DocumentArchive reopened = deserializeDocumentArchive(payload);
    if (!reopened.compatible
            || reopened.formatMagic != "iiPaintDocument"
            || reopened.formatVersion != documentArchiveFormatVersion
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
                                 {0xAA, 0xBB})
            || serializeDocumentArchive(reopened) != payload) {
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
    if (!migrated.compatible
            || migrated.formatVersion != documentArchiveFormatVersion
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
    if (!migratedPayload.ends_with("formatVersion\t4\n")
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

    // Run timed checks after opening the ordinary archive and exercising the error path once.
    const std::string header = "formatVersion\t3\n";
    if (!rejectsPromptly(header + "document.layers.count\t99999999999\n", "Size exceeds archive limit")
            || !rejectsPromptly(header + "document.layers.count\t-1\n", "Invalid unsigned decimal")
            || !rejectsPromptly(header + "document.layers.count\t1x\n", "Invalid unsigned decimal")
            || !rejectsPromptly(header + "assets.count\t18446744073709551616\n", "Size exceeds archive limit")
            || !rejectsPromptly(header + "history.undo.count\t3\n", "Insufficient archive fields")) {
        return 5;
    }

    // The hard depth limit still applies when a caller requests a higher value.
    const auto atDepthLimit = deserializeDocumentArchive(layerChainPayload(64));
    const auto *layers = &atDepthLimit.document.layers.layers;
    std::size_t depth = 0;
    while (layers->size() == 1) {
        ++depth;
        layers = &layers->front().children;
    }
    if (!atDepthLimit.compatible || depth != 64 || !layers->empty()
            || !rejectsPromptly(layerChainPayload(65), "Layer depth exceeds archive limit")
            || !rejectsPromptly(layerChainPayload(65), "Layer depth exceeds archive limit",
                                DocumentArchiveReadLimits{.maxLayerDepth = 65})) {
        return 6;
    }

    // Counts can use only their own contiguous keys. Other fields and duplicate records cannot fill gaps.
    const std::pair<std::string_view, std::string_view> missingItemCases[] = {
        {"document.layers.count\t2\ndocument.layers.0.surface.width\t0\n"
         "document.layers.0.surface.width\t0\nassets.0.id\t00\n", "document.layers.1.surface.width"},
        {"document.layers.count\t1\ndocument.layers.0.surface.width\t0\n"
         "document.layers.0.children.count\t1\n", "document.layers.0.children.0.surface.width"},
        {"document.surface.dirtyRegion.count\t1\n", "document.surface.dirtyRegion.0"},
        {"history.undo.count\t1\n", "history.undo.0.sequence"},
        {"history.redo.count\t1\n", "history.redo.0.sequence"},
        {"history.undo.count\t1\nhistory.undo.0.sequence\t1\n"
         "history.undo.0.patches.count\t1\n", "history.undo.0.patches.0.targetId"},
        {"brushSources.count\t1\n", "brushSources.0.brushId"},
        {"colorSpaces.count\t1\n", "colorSpaces.0.name"},
        {"assets.count\t1\n", "assets.0.id"},
    };
    for (const auto &[records, expectedError] : missingItemCases) {
        std::string malformed = header;
        malformed += records;
        if (!rejectsPromptly(malformed, expectedError)) {
            return 7;
        }
    }

    // A limit is shared across all lists of the corresponding type in a single read.
    if (!rejectsPromptly(payload, "document.layers.0.children.count", DocumentArchiveReadLimits{.maxLayers = 1})
            || !rejectsPromptly(payload, "document.layers.0.surface.dirtyRegion.count",
                                DocumentArchiveReadLimits{.maxDirtyRects = 1})
            || !rejectsPromptly(payload, "history.redo.count", DocumentArchiveReadLimits{.maxHistoryCommands = 1})
            || !rejectsPromptly(payload, "history.redo.0.patches.count", DocumentArchiveReadLimits{.maxHistoryPatches = 1})) {
        return 8;
    }

    std::string windowsPayload;
    for (const char character : legacyPayload) {
        if (character == '\n') {
            windowsPayload += '\r';
        }
        windowsPayload += character;
    }
    if (serializeDocumentArchive(deserializeDocumentArchive(windowsPayload)) != migratedPayload) {
        std::cerr << "Legacy CRLF archives must preserve all document records\n";
        return 9;
    }
    if (!checkBinaryFraming()) return 10;
    const bool hexadecimalChecksPassed = checkHexadecimalIntegrity();
    const bool surfaceChecksPassed = checkSurfacePixelCounts();
    if (!hexadecimalChecksPassed || !surfaceChecksPassed) return 11;
    return 0;
}
