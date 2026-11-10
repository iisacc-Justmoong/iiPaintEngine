//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <vector>

#include "Core/Types.h"
#include "Layer/DrawingSurface.h"
#include "Selection/Selection.h"

enum class FilterKind {
    Blur,
    Smudge,
};

struct FilterNode {
    bool enabled = true;
    FilterKind kind = FilterKind::Blur;
    Types::Scalar radius = 0.0;
    Types::Scalar strength = 1.0;
};

struct FilterPipeline {
    std::vector<FilterNode> nodes;
};

void applyFilterPipeline(DrawingSurface &surface,
                         const SelectionState &selection,
                         const FilterPipeline &pipeline);
