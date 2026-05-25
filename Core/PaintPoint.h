//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/CoordinateSpace.h"

template <typename Space>
struct PaintPoint {
    using SpaceType = Space;
    using Scalar = typename CoordinateScalar<Space>::Type;

    Scalar x{};
    Scalar y{};
};

using DocumentPoint = PaintPoint<DocumentCoordinateSpace>;
using CanvasPoint = DocumentPoint;
using ViewPoint = PaintPoint<ViewCoordinateSpace>;
using DevicePixelPoint = PaintPoint<DevicePixelCoordinateSpace>;
