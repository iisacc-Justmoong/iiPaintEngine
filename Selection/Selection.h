//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <vector>

#include "Core/PaintRect.h"
#include "Core/Types.h"

enum class SelectionShape {
    None,
    Rectangle,
    Mask,
};

enum class SelectionOperation {
    Replace,
    Add,
    Subtract,
    Intersect,
};

struct SelectionMask {
    DevicePixelRect bounds{};
    std::vector<Types::Byte> alpha;
};

struct SelectionState {
    bool active = false;
    SelectionShape shape = SelectionShape::None;
    DocumentRect bounds{};
    SelectionMask mask;
    Types::Scalar featherRadius = 0.0;
    bool inverted = false;
};

SelectionState makeRectangularSelection(DocumentRect bounds);

bool selectionContains(const SelectionState &selection, DocumentPoint point);

Types::Scalar selectionAlphaAt(const SelectionState &selection, DevicePixelPoint point);
