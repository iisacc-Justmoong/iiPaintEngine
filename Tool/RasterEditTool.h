//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <cstdint>

#include "Core/PaintPoint.h"
#include "Core/Types.h"
#include "Layer/DrawingSurface.h"
#include "Selection/Selection.h"

enum class GradientKind {
    Linear,
    Radial,
};

struct FillOperation {
    bool enabled = true;
    std::uint32_t argb = 0x00000000U;
    Types::Scalar opacity = 1.0;
};

struct GradientOperation {
    bool enabled = true;
    GradientKind kind = GradientKind::Linear;
    DocumentPoint start{};
    DocumentPoint end{};
    std::uint32_t startArgb = 0x00000000U;
    std::uint32_t endArgb = 0x00000000U;
    Types::Scalar opacity = 1.0;
};

struct EraserOperation {
    bool enabled = true;
    Types::Scalar opacity = 1.0;
};

void applyFill(DrawingSurface &surface, const SelectionState &selection, const FillOperation &operation);

void applyGradient(DrawingSurface &surface, const SelectionState &selection, const GradientOperation &operation);

void applyEraser(DrawingSurface &surface, const SelectionState &selection, const EraserOperation &operation);
