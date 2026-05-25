//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/PaintPoint.h"

template <typename Space>
struct PaintRect {
    using SpaceType = Space;
    using Scalar = typename CoordinateScalar<Space>::Type;

    PaintPoint<Space> origin{};
    Scalar width{};
    Scalar height{};
};

using DocumentRect = PaintRect<DocumentCoordinateSpace>;
using CanvasRect = DocumentRect;
using ViewRect = PaintRect<ViewCoordinateSpace>;
using DevicePixelRect = PaintRect<DevicePixelCoordinateSpace>;
