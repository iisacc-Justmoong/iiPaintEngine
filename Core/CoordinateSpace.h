//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"

enum class CoordinateSpaceKind {
    Canvas,
    View,
    DevicePixel,
};

struct CanvasCoordinateSpace {
    static constexpr CoordinateSpaceKind kind = CoordinateSpaceKind::Canvas;
};

struct ViewCoordinateSpace {
    static constexpr CoordinateSpaceKind kind = CoordinateSpaceKind::View;
};

struct DevicePixelCoordinateSpace {
    static constexpr CoordinateSpaceKind kind = CoordinateSpaceKind::DevicePixel;
};

template <typename Space>
struct CoordinateScalar {
    using Type = Types::Scalar;
};

template <>
struct CoordinateScalar<DevicePixelCoordinateSpace> {
    using Type = Types::Pixel;
};

struct CoordinateSpace {
    CoordinateSpaceKind kind = CoordinateSpaceKind::Canvas;
};
