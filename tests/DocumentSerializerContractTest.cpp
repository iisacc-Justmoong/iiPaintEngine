#include <cstddef>
#include <string>

#include "Canvas/Canvas.h"
#include "Brush/BrushSnapshot.h"
#include "Color/ColorSpace.h"
#include "Document/DocumentSerializer.h"
#include "Document/PaintDocument.h"
#include "History/Command.h"
#include "Layer/DrawingSurface.h"
#include "Layer/Layer.h"
#include "Stroke/StrokeCommand.h"

namespace {

PaintUuid uuidWithFirstByte(std::uint8_t value)
{
    PaintUuid uuid{};
    uuid.bytes[0] = value;
    return uuid;
}

bool byteVectorEquals(const std::vector<std::byte> &bytes, std::initializer_list<unsigned int> expected)
{
    if (bytes.size() != expected.size()) {
        return false;
    }

    std::size_t index = 0;
    for (const unsigned int expectedByte : expected) {
        if (std::to_integer<unsigned int>(bytes[index]) != expectedByte) {
            return false;
        }
        ++index;
    }
    return true;
}

} // namespace

int main()
{
    DrawingSurface surface = makeDrawingSurface(3, 2, 0x00000000U);
    surface.pixels[4] = 0xFF224466U;

    PaintDocument document;
    document.metadata.title = "RoundTrip";
    document.metadata.author = "Painter";
    document.metadata.documentId = uuidWithFirstByte(1);
    document.metadata.storagePath = "roundtrip.ipe";

    CanvasMetadata canvasMetadata;
    canvasMetadata.title = "Canvas A";
    canvasMetadata.colorSpace = "Display P3";
    canvasMetadata.backgroundColor = 0xFF010203U;

    Canvas canvas = makeCanvas(surface, canvasMetadata);
    Layer layer;
    layer.surface = surface;
    layer.metadata.id = uuidWithFirstByte(2);
    layer.metadata.name = "Ink";
    layer.metadata.opacity = 0.75;
    layer.metadata.blendMode = RasterBlendMode::SourceOver;
    canvas.layers.layers.push_back(layer);

    StrokeInput input;
    input.points.push_back(StrokePoint{{1.0, 1.0}, 0.5, 0.0, 2.0, 0.1, 0.2, 3, 0.0});
    input.points.push_back(StrokePoint{{2.0, 1.0}, 0.8, 1.0, 3.0, 0.2, 0.3, 4, 1.0});

    BrushState brush;
    brush.randomSeed = 42;
    brush.rasterizer.argb = 0xFF336699U;
    brush.rasterizer.brushSize = 6.0;
    brush.rasterizer.flow = 0.6;
    brush.dynamics.pressureToSize = 0.5;
    brush.resampler.sampleSpacing = 0.25;
    canvas.strokes.strokes.push_back(makeStrokeCommand(input, brush, Stabilizer{0.0}));
    document.canvases.push_back(canvas);

    BrushSnapshot brushSource;
    brushSource.brushId = uuidWithFirstByte(3);
    brushSource.size = 6.0F;
    brushSource.opacity = 0.9F;
    brushSource.tip.width = 2;
    brushSource.tip.height = 2;
    brushSource.tip.mask = {std::byte{0x00}, std::byte{0x7F}, std::byte{0xCC}, std::byte{0xFF}};

    ColorSpace colorSpace;
    colorSpace.name = "Display P3";
    colorSpace.iccProfile = {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};

    DocumentAsset asset;
    asset.id = uuidWithFirstByte(4);
    asset.name = "paper";
    asset.mimeType = "image/png";
    asset.bytes = {std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47}};

    Command historyCommand;
    historyCommand.sequence = 1;
    historyCommand.label = "Paint stroke";
    historyCommand.targetId = layer.metadata.id;

    DocumentArchive archive = makeDocumentArchive(document);
    archive.brushSources.push_back(brushSource);
    archive.colorSpaces.push_back(colorSpace);
    archive.assets.push_back(asset);
    archive.history.undoCommands.push_back(historyCommand);
    archive.history.cursor = 1;

    const std::string payload = serializeDocumentArchive(archive);
    const DocumentArchive reopened = deserializeDocumentArchive(payload);

    if (reopened.formatMagic != "iiPaintDocument"
            || reopened.formatVersion != 1
            || reopened.document.metadata.title != "RoundTrip"
            || reopened.document.canvases.size() != 1
            || reopened.document.canvases.front().layers.layers.size() != 1
            || reopened.document.canvases.front().strokes.strokes.size() != 1) {
        return 1;
    }

    const Layer &reopenedLayer = reopened.document.canvases.front().layers.layers.front();
    const StrokeCommand &reopenedStroke = reopened.document.canvases.front().strokes.strokes.front();
    if (reopenedLayer.metadata.id.bytes[0] != 2
            || reopenedLayer.metadata.name != "Ink"
            || reopenedLayer.metadata.opacity != 0.75
            || drawingSurfacePixelAt(reopenedLayer.surface, {1, 1}) != 0xFF224466U
            || reopenedStroke.path.rawInput.points.size() != 2
            || reopenedStroke.brush.randomSeed != 42
            || reopenedStroke.brush.rasterizer.argb != 0xFF336699U
            || reopenedStroke.brush.dynamics.pressureToSize != 0.5
            || reopenedStroke.brush.resampler.sampleSpacing != 0.25) {
        return 1;
    }

    if (reopened.brushSources.size() != 1
            || reopened.brushSources.front().brushId.bytes[0] != 3
            || !byteVectorEquals(reopened.brushSources.front().tip.mask, {0x00, 0x7F, 0xCC, 0xFF})
            || reopened.colorSpaces.size() != 1
            || reopened.colorSpaces.front().name != "Display P3"
            || !byteVectorEquals(reopened.colorSpaces.front().iccProfile, {0x10, 0x20, 0x30})
            || reopened.assets.size() != 1
            || reopened.assets.front().mimeType != "image/png"
            || !byteVectorEquals(reopened.assets.front().bytes, {0x89, 0x50, 0x4E, 0x47})
            || reopened.history.cursor != 1
            || reopened.history.undoCommands.size() != 1
            || reopened.history.undoCommands.front().targetId.bytes[0] != 2) {
        return 1;
    }

    return 0;
}
