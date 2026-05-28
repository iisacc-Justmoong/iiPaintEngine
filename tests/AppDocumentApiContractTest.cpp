#include <cstdint>
#include <string>
#include <vector>

#include "QtAdapter/DocumentAdapter.h"
#include "QtAdapter/LayerListModel.h"
#include "QtAdapter/PaintEngineController.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCommand.h"

namespace {

StrokeCommand makeTestStrokeCommand()
{
    StrokeInput input;
    input.points = {
            {{1.0, 1.0}, 1.0, 0.0},
            {{3.0, 1.0}, 1.0, 1.0},
    };

    BrushState brush;
    brush.randomSeed = 17;
    brush.rasterizer.argb = 0xFF336699U;
    brush.rasterizer.radius = 1;
    brush.rasterizer.spacing = 1.0;
    brush.rasterizer.flow = 1.0;
    brush.rasterizer.opacity = 1.0;

    return makeStrokeCommand(input, brush, Stabilizer{0.0});
}

} // namespace

int main()
{
    PaintEngineController controller = makePaintEngineController(8, 6, 0x00000000U);
    if (!controller.documentOpen
            || documentCanvasCount(controller.document) != 1
            || documentLayerCount(controller.document) != 1
            || controller.layers.rows.size() != 1
            || !controller.layers.rows.front().active) {
        return 1;
    }

    if (!renameDocumentLayer(controller.document, 0, "Ink")
            || activeDocumentLayer(controller.document)->metadata.name != "Ink") {
        return 2;
    }

    refreshPaintEngineControllerLayerList(controller);
    if (controller.layers.rows.front().name != "Ink") {
        return 3;
    }

    if (!addPaintLayer(controller, "Sketch", 0x00000000U)
            || documentLayerCount(controller.document) != 2
            || activeDocumentCanvas(controller.document)->layers.activeLayerIndex != 1
            || controller.layers.rows.size() != 2
            || !controller.layers.rows.back().active) {
        return 4;
    }

    if (!setDocumentLayerVisible(controller.document, 1, false)
            || !setDocumentLayerOpacity(controller.document, 1, 0.5)
            || activeDocumentLayer(controller.document)->metadata.visible
            || activeDocumentLayer(controller.document)->metadata.opacity != 0.5) {
        return 5;
    }

    if (!selectPaintLayer(controller, 0)
            || activeDocumentCanvas(controller.document)->layers.activeLayerIndex != 0) {
        return 6;
    }

    const StrokeCommand command = makeTestStrokeCommand();
    const std::vector<RasterSample> samples = projectBrushDabs(command.dabs, command.brush.rasterizer);
    if (!commitPaintStroke(controller, command, samples)
            || activeDocumentCanvas(controller.document)->strokes.strokes.size() != 1
            || controller.document.archive.history.undoCommands.size() != 1
            || controller.document.archive.history.undoCommands.front().kind != CommandKind::PaintStroke
            || controller.layers.rows.front().name != "Ink") {
        return 7;
    }

    const Layer *ink = documentLayerAt(controller.document, 0);
    if (ink == nullptr || drawingSurfacePixelAt(ink->surface, {1, 1}) != 0xFF336699U) {
        return 8;
    }

    const std::string payload = savePaintDocumentArchive(controller);
    PaintEngineController reopened;
    if (payload.empty()
            || !openPaintDocumentArchive(reopened, payload)
            || !reopened.documentOpen
            || documentLayerCount(reopened.document) != 2
            || activeDocumentCanvas(reopened.document)->strokes.strokes.size() != 1
            || reopened.document.archive.history.undoCommands.size() != 1
            || reopened.layers.rows.front().name != "Ink") {
        return 9;
    }

    const Layer *reopenedInk = documentLayerAt(reopened.document, 0);
    if (reopenedInk == nullptr
            || drawingSurfacePixelAt(reopenedInk->surface, {1, 1}) != 0xFF336699U) {
        return 10;
    }

    if (!newPaintDocument(reopened, 4, 4, 0xFFFFFFFFU)
            || documentLayerCount(reopened.document) != 1
            || reopened.layers.rows.size() != 1
            || reopened.document.archive.history.undoCommands.size() != 0
            || drawingSurfacePixelAt(activeDocumentLayer(reopened.document)->surface, {0, 0}) != 0xFFFFFFFFU) {
        return 11;
    }

    return 0;
}
