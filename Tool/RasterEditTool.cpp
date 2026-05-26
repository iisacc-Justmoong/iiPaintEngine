//
// Created by Justmoong on 2026 May 26.
//

#include "RasterEditTool.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

std::size_t surfaceIndex(const DrawingSurface &surface, DevicePixelPoint point)
{
    return static_cast<std::size_t>(point.y) * static_cast<std::size_t>(surface.width)
            + static_cast<std::size_t>(point.x);
}

std::uint8_t channel(std::uint32_t argb, unsigned shift)
{
    return static_cast<std::uint8_t>((argb >> shift) & 0xFFU);
}

std::uint8_t mixByte(std::uint8_t start, std::uint8_t end, Types::Scalar t)
{
    return static_cast<std::uint8_t>(std::clamp<int>(
            static_cast<int>(std::lround(static_cast<Types::Scalar>(start)
                                         + (static_cast<Types::Scalar>(end) - static_cast<Types::Scalar>(start)) * t)),
            0,
            255));
}

std::uint32_t mixArgb(std::uint32_t start, std::uint32_t end, Types::Scalar t)
{
    const Types::Scalar clamped = std::clamp(t, 0.0, 1.0);
    return (static_cast<std::uint32_t>(mixByte(channel(start, 24U), channel(end, 24U), clamped)) << 24U)
            | (static_cast<std::uint32_t>(mixByte(channel(start, 16U), channel(end, 16U), clamped)) << 16U)
            | (static_cast<std::uint32_t>(mixByte(channel(start, 8U), channel(end, 8U), clamped)) << 8U)
            | static_cast<std::uint32_t>(mixByte(channel(start, 0U), channel(end, 0U), clamped));
}

Types::Scalar linearGradientT(const GradientOperation &operation, DevicePixelPoint point)
{
    const Types::Scalar dx = operation.end.x - operation.start.x;
    const Types::Scalar dy = operation.end.y - operation.start.y;
    const Types::Scalar lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0) {
        return 0.0;
    }

    const Types::Scalar px = static_cast<Types::Scalar>(point.x) - operation.start.x;
    const Types::Scalar py = static_cast<Types::Scalar>(point.y) - operation.start.y;
    return std::clamp((px * dx + py * dy) / lengthSquared, 0.0, 1.0);
}

std::uint32_t erasedArgb(std::uint32_t argb, Types::Scalar opacity)
{
    const auto alpha = static_cast<Types::Scalar>(channel(argb, 24U));
    const auto nextAlpha = static_cast<std::uint8_t>(std::clamp<int>(
            static_cast<int>(std::lround(alpha * (1.0 - std::clamp(opacity, 0.0, 1.0)))),
            0,
            255));
    return (argb & 0x00FFFFFFU) | (static_cast<std::uint32_t>(nextAlpha) << 24U);
}

template <typename Callback>
void forSelectedPixels(DrawingSurface &surface, const SelectionState &selection, Callback callback)
{
    for (Types::Pixel y = 0; y < surface.height; ++y) {
        for (Types::Pixel x = 0; x < surface.width; ++x) {
            const DevicePixelPoint point{x, y};
            if (selectionAlphaAt(selection, point) <= 0.0) {
                continue;
            }
            callback(point, surfaceIndex(surface, point));
        }
    }
}

} // namespace

void applyFill(DrawingSurface &surface, const SelectionState &selection, const FillOperation &operation)
{
    forSelectedPixels(surface, selection, [&](DevicePixelPoint, std::size_t index) {
        surface.pixels[index] = operation.argb;
    });
}

void applyGradient(DrawingSurface &surface, const SelectionState &selection, const GradientOperation &operation)
{
    forSelectedPixels(surface, selection, [&](DevicePixelPoint point, std::size_t index) {
        const Types::Scalar t = operation.kind == GradientKind::Linear ? linearGradientT(operation, point) : 0.0;
        surface.pixels[index] = mixArgb(operation.startArgb, operation.endArgb, t);
    });
}

void applyEraser(DrawingSurface &surface, const SelectionState &selection, const EraserOperation &operation)
{
    forSelectedPixels(surface, selection, [&](DevicePixelPoint, std::size_t index) {
        surface.pixels[index] = erasedArgb(surface.pixels[index], operation.opacity);
    });
}
