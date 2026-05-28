//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Core/PaintUuid.h"
#include "Layer/LayerMetadata.h"
#include "Layer/LayerStack.h"
#include "QtAdapter/DocumentAdapter.h"

struct LayerListRow {
    PaintUuid id;
    std::string name;
    bool visible = true;
    Types::Scalar opacity = 1.0;
    LayerKind kind = LayerKind::Paint;
    std::size_t sourceIndex = 0;
    std::size_t depth = 0;
    bool active = false;
};

struct LayerListModel {
    std::vector<LayerListRow> rows;
    std::size_t activeLayerIndex = 0;
};

LayerListModel makeLayerListModel(const LayerStack &layers);

LayerListModel makeLayerListModel(const DocumentAdapter &adapter);

const LayerListRow *layerListRowAt(const LayerListModel &model, std::size_t rowIndex);
