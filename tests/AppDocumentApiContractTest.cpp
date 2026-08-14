#include <cstdint>
#include <string>
#include <vector>

#include "QtAdapter/DocumentAdapter.h"
#include "QtAdapter/LayerListModel.h"
#include "QtAdapter/PaintEngineController.h"

int main()
{
    PaintEngineController controller = makePaintEngineController(8, 6, 0x00000000U);
    if (!controller.documentOpen
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

    if (!addPaintLayer(controller, "Sketch", 0x00000000U)
            || documentLayerCount(controller.document) != 2
            || controller.document.archive.document.layers.activeLayerIndex != 1) {
        return 3;
    }
    if (!setDocumentLayerVisible(controller.document, 1, false)
            || !setDocumentLayerOpacity(controller.document, 1, 0.5)
            || !selectPaintLayer(controller, 0)) {
        return 4;
    }

    const std::vector<RasterSample> pixels{
            RasterSample{{1, 1}, 0xFF336699U},
            RasterSample{{2, 1}, 0xFF336699U},
            RasterSample{{3, 1}, 0xFF336699U},
    };
    if (!commitPaintSamples(controller, pixels, {{1.0, 1.0}, 3.0, 1.0})
            || controller.document.archive.history.undoCommands.size() != 1
            || controller.document.archive.history.undoCommands.front().kind != CommandKind::PaintStroke
            || controller.document.archive.history.undoCommands.front().dirtyBounds.width != 3.0) {
        return 5;
    }

    const Layer *ink = documentLayerAt(controller.document, 0);
    if (ink == nullptr || drawingSurfacePixelAt(ink->surface, {1, 1}) != 0xFF336699U) {
        return 6;
    }

    const std::string payload = savePaintDocumentArchive(controller);
    PaintEngineController reopened;
    if (payload.empty()
            || payload.find("strokes") != std::string::npos
            || !openPaintDocumentArchive(reopened, payload)
            || documentLayerCount(reopened.document) != 2
            || reopened.document.archive.history.undoCommands.size() != 1
            || reopened.layers.rows.front().name != "Ink") {
        return 7;
    }

    const Layer *reopenedInk = documentLayerAt(reopened.document, 0);
    if (reopenedInk == nullptr
            || drawingSurfacePixelAt(reopenedInk->surface, {1, 1}) != 0xFF336699U) {
        return 8;
    }

    if (!newPaintDocument(reopened, 4, 4, 0xFFFFFFFFU)
            || documentLayerCount(reopened.document) != 1
            || reopened.document.archive.history.undoCommands.size() != 0
            || drawingSurfacePixelAt(activeDocumentLayer(reopened.document)->surface, {0, 0}) != 0xFFFFFFFFU) {
        return 9;
    }
    return 0;
}
