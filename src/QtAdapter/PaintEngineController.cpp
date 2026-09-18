//
// Created by Justmoong on 2026 May 24.
//

#include "PaintEngineController.h"

PaintEngineController makePaintEngineController(Types::Pixel width,
                                                Types::Pixel height,
                                                std::uint32_t clearArgb)
{
    PaintEngineController controller;
    newPaintDocument(controller, width, height, clearArgb);
    return controller;
}

bool newPaintDocument(PaintEngineController &controller,
                      Types::Pixel width,
                      Types::Pixel height,
                      std::uint32_t clearArgb)
{
    controller.document = makeDocumentAdapter(width, height, clearArgb);
    controller.documentOpen = documentAdapterHasDocument(controller.document);
    refreshPaintEngineControllerLayerList(controller);
    return controller.documentOpen;
}

bool openPaintDocumentArchive(PaintEngineController &controller, const std::string &payload)
{
    DocumentAdapter adapter;
    if (!loadDocumentArchivePayload(adapter, payload)) {
        return false;
    }

    controller.document = adapter;
    controller.documentOpen = true;
    refreshPaintEngineControllerLayerList(controller);
    return true;
}

std::string savePaintDocumentArchive(const PaintEngineController &controller)
{
    if (!controller.documentOpen) {
        return {};
    }
    return saveDocumentArchivePayload(controller.document);
}

void refreshPaintEngineControllerLayerList(PaintEngineController &controller)
{
    controller.layers = makeLayerListModel(controller.document);
}

bool addPaintLayer(PaintEngineController &controller,
                   const std::string &name,
                   std::uint32_t clearArgb)
{
    if (!controller.documentOpen || !addDocumentRasterLayer(controller.document, name, clearArgb)) {
        return false;
    }

    refreshPaintEngineControllerLayerList(controller);
    return true;
}

bool selectPaintLayer(PaintEngineController &controller, std::size_t layerIndex)
{
    if (!controller.documentOpen || !selectDocumentLayer(controller.document, layerIndex)) {
        return false;
    }

    refreshPaintEngineControllerLayerList(controller);
    return true;
}

bool commitPaintSamples(PaintEngineController &controller,
                        const std::vector<RasterSample> &samples,
                        DocumentRect dirtyBounds)
{
    if (!controller.documentOpen
            || !commitRasterSamplesToActiveDocumentLayer(controller.document, samples, dirtyBounds)) {
        return false;
    }

    refreshPaintEngineControllerLayerList(controller);
    return true;
}
