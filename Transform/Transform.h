//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include "Core/PaintPoint.h"
#include "Core/PaintRect.h"
#include "Layer/DrawingSurface.h"
#include "Selection/Selection.h"

enum class TransformInterpolation {
    Nearest,
    Bilinear,
};

struct AffineTransform {
    Types::Scalar m11 = 1.0;
    Types::Scalar m12 = 0.0;
    Types::Scalar m21 = 0.0;
    Types::Scalar m22 = 1.0;
    Types::Scalar translationX = 0.0;
    Types::Scalar translationY = 0.0;
};

struct TransformState {
    AffineTransform transform;
    TransformInterpolation interpolation = TransformInterpolation::Nearest;
    DocumentRect sourceBounds{};
    DocumentRect transformedBounds{};
    bool active = false;
};

AffineTransform makeTranslationTransform(Types::Scalar dx, Types::Scalar dy);

DocumentPoint transformPoint(AffineTransform transform, DocumentPoint point);

DocumentRect transformRect(AffineTransform transform, DocumentRect rect);

SelectionState transformSelection(const SelectionState &selection, AffineTransform transform);

DrawingSurface cropDrawingSurface(const DrawingSurface &surface, DevicePixelRect cropRect);
