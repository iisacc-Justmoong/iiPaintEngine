//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "QtAdapter/DocumentAdapter.h"
#include "QtAdapter/LayerListModel.h"

struct PaintEngineController {
    DocumentAdapter document;
    LayerListModel layers;
    bool documentOpen = false;
};

PaintEngineController makePaintEngineController(Types::Pixel width,
                                                Types::Pixel height,
                                                std::uint32_t clearArgb = 0x00000000U);

bool newPaintDocument(PaintEngineController &controller,
                      Types::Pixel width,
                      Types::Pixel height,
                      std::uint32_t clearArgb = 0x00000000U);

bool openPaintDocumentArchive(PaintEngineController &controller, const std::string &payload);

std::string savePaintDocumentArchive(const PaintEngineController &controller);

void refreshPaintEngineControllerLayerList(PaintEngineController &controller);

bool addPaintLayer(PaintEngineController &controller,
                   const std::string &name,
                   std::uint32_t clearArgb = 0x00000000U);

bool selectPaintLayer(PaintEngineController &controller, std::size_t layerIndex);

bool commitPaintSamples(PaintEngineController &controller,
                        const std::vector<RasterSample> &samples,
                        DocumentRect dirtyBounds = {});
