//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/PaintRect.h"
#include "Core/RasterSample.h"
#include "Document/DocumentSerializer.h"
#include "Layer/Layer.h"

struct DocumentAdapter {
    DocumentArchive archive;
};

DocumentAdapter makeDocumentAdapter(Types::Pixel width,
                                    Types::Pixel height,
                                    std::uint32_t clearArgb = 0x00000000U);

bool loadDocumentArchivePayload(DocumentAdapter &adapter, const std::string &payload);

std::string saveDocumentArchivePayload(const DocumentAdapter &adapter);

bool documentAdapterHasDocument(const DocumentAdapter &adapter);

std::size_t documentLayerCount(const DocumentAdapter &adapter);

bool selectDocumentLayer(DocumentAdapter &adapter, std::size_t layerIndex);

Layer *activeDocumentLayer(DocumentAdapter &adapter);

const Layer *activeDocumentLayer(const DocumentAdapter &adapter);

Layer *documentLayerAt(DocumentAdapter &adapter, std::size_t layerIndex);

const Layer *documentLayerAt(const DocumentAdapter &adapter, std::size_t layerIndex);

bool addDocumentRasterLayer(DocumentAdapter &adapter,
                            const std::string &name,
                            std::uint32_t clearArgb = 0x00000000U,
                            bool selectCreatedLayer = true);

bool renameDocumentLayer(DocumentAdapter &adapter, std::size_t layerIndex, const std::string &name);

bool setDocumentLayerVisible(DocumentAdapter &adapter, std::size_t layerIndex, bool visible);

bool setDocumentLayerOpacity(DocumentAdapter &adapter, std::size_t layerIndex, Types::Scalar opacity);

bool commitRasterSamplesToActiveDocumentLayer(DocumentAdapter &adapter,
                                              const std::vector<RasterSample> &samples,
                                              DocumentRect dirtyBounds = {});

void refreshDocumentSurface(DocumentAdapter &adapter);
