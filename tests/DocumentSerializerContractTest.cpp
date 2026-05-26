#include <cstddef>
#include <string>

#include "Canvas/Canvas.h"
#include "Brush/BrushSnapshot.h"
#include "Color/ColorSpace.h"
#include "Document/DocumentSerializer.h"
#include "Document/PaintDocument.h"
#include "History/Command.h"
#include "Input/PointerEvent.h"
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

bool byteVectorEquals(const std::vector<Types::Byte> &bytes, std::initializer_list<unsigned int> expected)
{
    if (bytes.size() != expected.size()) {
        return false;
    }

    std::size_t index = 0;
    for (const unsigned int expectedByte : expected) {
        if (bytes[index] != expectedByte) {
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
    layer.metadata.alphaLock = true;
    layer.mask.enabled = true;
    layer.mask.width = 3;
    layer.mask.height = 2;
    layer.mask.alpha = {0xFF, 0x80, 0x00, 0x40, 0xFF, 0x20};
    Layer childLayer;
    childLayer.metadata.kind = LayerKind::Adjustment;
    childLayer.metadata.name = "Tone";
    layer.children.push_back(childLayer);
    canvas.layers.layers.push_back(layer);

    StrokeInput input;
    input.points.push_back(StrokePoint{{1.0, 1.0}, 0.5, 0.0, 2.0, 0.1, 0.2, 3, 0.0});
    input.points.push_back(StrokePoint{{2.0, 1.0}, 0.8, 1.0, 3.0, 0.2, 0.3, 4, 1.0});
    input.points.front().deviceState = PointerDeviceStateBarrelButton;
    input.points.front().rotationRadians = 0.35;
    input.points.back().deviceState = PointerDeviceStateEraser;
    input.points.back().rotationRadians = 0.55;

    BrushState brush;
    brush.randomSeed = 42;
    brush.rasterizer.argb = 0xFF336699U;
    brush.rasterizer.brushSize = 6.0;
    brush.rasterizer.flow = 0.6;
    brush.rasterizer.spacingEnabled = false;
    brush.rasterizer.flowEnabled = false;
    brush.rasterizer.opacityEnabled = false;
    brush.rasterizer.hardnessEnabled = false;
    brush.dynamics.pressureToSize = 0.5;
    brush.dynamics.pressureToFlowEnabled = false;
    brush.dynamics.velocityToDryOutEnabled = false;
    brush.dynamics.tiltToEllipseEnabled = false;
    brush.dynamics.randomInputEnabled = false;
    brush.resampler.sampleSpacing = 0.25;
    brush.material.texture.enabled = true;
    brush.material.texture.width = 2;
    brush.material.texture.height = 1;
    brush.material.texture.alpha = {128, 255};
    brush.material.texture.grainStrength = 0.5;
    brush.material.dualBrush.enabled = true;
    brush.material.dualBrush.scale = 0.7;
    brush.material.scatter.enabled = true;
    brush.material.scatter.radius = 1.0;
    brush.material.scatter.count = 2;
    brush.material.simulation.enabled = true;
    brush.material.simulation.model = BrushSimulationModel::Smudge;
    brush.material.simulation.smudgeStrength = 0.4;
    brush.material.bristle.enabled = true;
    brush.material.bristle.shape = BristleShape::Fan;
    brush.material.bristle.count = 9;
    canvas.strokes.strokes.push_back(makeStrokeCommand(input, brush, Stabilizer{0.0}));
    document.canvases.push_back(canvas);

    BrushSnapshot brushSource;
    brushSource.brushId = uuidWithFirstByte(3);
    brushSource.name = "Archive Brush";
    brushSource.size = 6.0F;
    brushSource.opacity = 0.9F;
    brushSource.tip.width = 2;
    brushSource.tip.height = 2;
    brushSource.tip.mask = {std::byte{0x00}, std::byte{0x7F}, std::byte{0xCC}, std::byte{0xFF}};
    brushSource.material = brush.material;
    brushSource.material.simulation.model = BrushSimulationModel::Mixer;

    ColorSpace colorSpace = makeDisplayP3LinearFloatColorSpace();
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
    historyCommand.kind = CommandKind::PaintStroke;
    historyCommand.scope = CommandScope::Layer;
    historyCommand.transactionId = uuidWithFirstByte(5);
    historyCommand.timestamp = 42.0;
    historyCommand.coalescingKey = "layer:ink:stroke";
    historyCommand.dirtyBounds = {{0.0, 0.0}, 3.0, 2.0};
    CommandPatch historyPatch;
    historyPatch.targetId = layer.metadata.id;
    historyPatch.scope = CommandScope::Layer;
    historyPatch.dirtyBounds = historyCommand.dirtyBounds;
    historyPatch.beforeState.storage = CommandPayloadStorage::InlineBytes;
    historyPatch.beforeState.mimeType = "application/x-iipaint-rgba-tile";
    historyPatch.beforeState.bytes = {std::byte{0x00}, std::byte{0x11}};
    historyPatch.afterState.storage = CommandPayloadStorage::InlineBytes;
    historyPatch.afterState.mimeType = "application/x-iipaint-rgba-tile";
    historyPatch.afterState.bytes = {std::byte{0xAA}, std::byte{0xBB}};
    historyCommand.patches.push_back(historyPatch);

    DocumentArchive archive = makeDocumentArchive(document);
    archive.brushSources.push_back(brushSource);
    archive.colorSpaces.push_back(colorSpace);
    archive.assets.push_back(asset);
    archive.history.undoCommands.push_back(historyCommand);
    archive.history.cursor = 1;
    archive.history.nextSequence = 2;
    archive.history.maxUndoCommands = 128;

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
            || !reopenedLayer.metadata.alphaLock
            || !reopenedLayer.mask.enabled
            || !byteVectorEquals(reopenedLayer.mask.alpha, {0xFF, 0x80, 0x00, 0x40, 0xFF, 0x20})
            || reopenedLayer.children.size() != 1
            || reopenedLayer.children.front().metadata.kind != LayerKind::Adjustment
            || reopenedLayer.children.front().metadata.name != "Tone"
            || drawingSurfacePixelAt(reopenedLayer.surface, {1, 1}) != 0xFF224466U
            || reopenedStroke.path.rawInput.points.size() != 2
            || reopenedStroke.brush.randomSeed != 42
            || reopenedStroke.brush.rasterizer.argb != 0xFF336699U
            || reopenedStroke.brush.rasterizer.spacingEnabled
            || reopenedStroke.brush.rasterizer.flowEnabled
            || reopenedStroke.brush.rasterizer.opacityEnabled
            || reopenedStroke.brush.rasterizer.hardnessEnabled
            || reopenedStroke.brush.dynamics.pressureToSize != 0.5
            || reopenedStroke.brush.dynamics.pressureToFlowEnabled
            || reopenedStroke.brush.dynamics.velocityToDryOutEnabled
            || reopenedStroke.brush.dynamics.tiltToEllipseEnabled
            || reopenedStroke.brush.dynamics.randomInputEnabled
            || reopenedStroke.brush.resampler.sampleSpacing != 0.25
            || !reopenedStroke.brush.material.texture.enabled
            || !byteVectorEquals(reopenedStroke.brush.material.texture.alpha, {0x80, 0xFF})
            || !reopenedStroke.brush.material.dualBrush.enabled
            || reopenedStroke.brush.material.scatter.count != 2
            || !reopenedStroke.brush.material.simulation.enabled
            || reopenedStroke.brush.material.simulation.model != BrushSimulationModel::Smudge
            || !reopenedStroke.brush.material.bristle.enabled
            || reopenedStroke.brush.material.bristle.shape != BristleShape::Fan
            || reopenedStroke.brush.material.bristle.count != 9
            || reopenedStroke.path.rawInput.points.front().deviceState != PointerDeviceStateBarrelButton
            || reopenedStroke.path.rawInput.points.front().rotationRadians != 0.35
            || reopenedStroke.dabs.empty()
            || reopenedStroke.dabs.front().textureAlpha <= 0.0
            || reopenedStroke.dabs.front().hardnessScale != 1.0
            || !reopenedStroke.dabs.front().dualBrush) {
        return 1;
    }

    if (reopened.brushSources.size() != 1
            || reopened.brushSources.front().brushId.bytes[0] != 3
            || reopened.brushSources.front().name != "Archive Brush"
            || !byteVectorEquals(reopened.brushSources.front().tip.mask, {0x00, 0x7F, 0xCC, 0xFF})
            || !reopened.brushSources.front().material.simulation.enabled
            || reopened.brushSources.front().material.simulation.model != BrushSimulationModel::Mixer
            || !reopened.brushSources.front().material.bristle.enabled
            || reopened.brushSources.front().material.bristle.count != 9
            || reopened.colorSpaces.size() != 1
            || reopened.colorSpaces.front().name != "Display P3 Linear Float"
            || reopened.colorSpaces.front().primaries != ColorPrimaries::DisplayP3
            || reopened.colorSpaces.front().transferFunction != ColorTransferFunction::Linear
            || reopened.colorSpaces.front().componentEncoding != ColorComponentEncoding::Float32
            || !reopened.colorSpaces.front().hdr
            || !reopened.colorSpaces.front().linear
            || reopened.colorSpaces.front().maxComponentValue != 16.0
            || !byteVectorEquals(reopened.colorSpaces.front().iccProfile, {0x10, 0x20, 0x30})
            || reopened.assets.size() != 1
            || reopened.assets.front().mimeType != "image/png"
            || !byteVectorEquals(reopened.assets.front().bytes, {0x89, 0x50, 0x4E, 0x47})
            || reopened.history.cursor != 1
            || reopened.history.nextSequence != 2
            || reopened.history.maxUndoCommands != 128
            || reopened.history.undoCommands.size() != 1
            || reopened.history.undoCommands.front().targetId.bytes[0] != 2
            || reopened.history.undoCommands.front().kind != CommandKind::PaintStroke
            || reopened.history.undoCommands.front().scope != CommandScope::Layer
            || reopened.history.undoCommands.front().transactionId.bytes[0] != 5
            || reopened.history.undoCommands.front().coalescingKey != "layer:ink:stroke"
            || reopened.history.undoCommands.front().dirtyBounds.width != 3.0
            || reopened.history.undoCommands.front().patches.size() != 1
            || reopened.history.undoCommands.front().patches.front().beforeState.storage != CommandPayloadStorage::InlineBytes
            || !byteVectorEquals(reopened.history.undoCommands.front().patches.front().beforeState.bytes, {0x00, 0x11})
            || !byteVectorEquals(reopened.history.undoCommands.front().patches.front().afterState.bytes, {0xAA, 0xBB})) {
        return 1;
    }

    return 0;
}
