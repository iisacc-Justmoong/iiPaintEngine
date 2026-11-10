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

// Fill/gradient use straight-ARGB source-over with opacity * selection coverage.
// Opacity is clamped to [0,1]; non-finite opacity and disabled operations are no-ops.
void applyFill(DrawingSurface &surface, const SelectionState &selection, const FillOperation &operation);

// Radial: start is the center, |end-start| is the radius. Zero-length gradients
// use the first color; non-finite coordinates/spans leave the surface unchanged.
void applyGradient(DrawingSurface &surface, const SelectionState &selection, const GradientOperation &operation);

void applyEraser(DrawingSurface &surface, const SelectionState &selection, const EraserOperation &operation);
