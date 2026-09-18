//
// Created by Justmoong on 2026 May 24.
//

#include "LayerListModel.h"

namespace {

void appendLayerRows(LayerListModel &model,
                     const std::vector<Layer> &layers,
                     std::size_t activeLayerIndex,
                     std::size_t depth)
{
    for (std::size_t index = 0; index < layers.size(); ++index) {
        const Layer &layer = layers[index];
        LayerListRow row;
        row.id = layer.metadata.id;
        row.name = layer.metadata.name;
        row.visible = layer.metadata.visible;
        row.opacity = layer.metadata.opacity;
        row.kind = layer.metadata.kind;
        row.sourceIndex = index;
        row.depth = depth;
        row.active = depth == 0 && index == activeLayerIndex;
        model.rows.push_back(row);

        if (!layer.children.empty()) {
            appendLayerRows(model, layer.children, 0, depth + 1U);
        }
    }
}

} // namespace

LayerListModel makeLayerListModel(const LayerStack &layers)
{
    LayerListModel model;
    model.activeLayerIndex = layers.activeLayerIndex;
    appendLayerRows(model, layers.layers, layers.activeLayerIndex, 0);
    return model;
}

LayerListModel makeLayerListModel(const DocumentAdapter &adapter)
{
    if (!documentAdapterHasDocument(adapter)) {
        return {};
    }
    return makeLayerListModel(adapter.archive.document.layers);
}

const LayerListRow *layerListRowAt(const LayerListModel &model, std::size_t rowIndex)
{
    if (rowIndex >= model.rows.size()) {
        return nullptr;
    }
    return &model.rows[rowIndex];
}
