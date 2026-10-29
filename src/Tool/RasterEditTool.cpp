//
// Created by Justmoong on 2026 May 26.
//

#include "RasterEditTool.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Render/Compositor.h"

namespace {

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
    const double length = std::hypot(dx, dy);
    if (length <= 0.0) {
        return 0.0;
    }

    const Types::Scalar px = static_cast<Types::Scalar>(point.x) - operation.start.x;
    const Types::Scalar py = static_cast<Types::Scalar>(point.y) - operation.start.y;
    return std::clamp((px * (dx / length) + py * (dy / length)) / length, 0.0, 1.0);
}

Types::Scalar radialGradientT(const GradientOperation &operation, DevicePixelPoint point)
{
    const double radius = std::hypot(operation.end.x - operation.start.x,
                                     operation.end.y - operation.start.y);
    if (radius <= 0) return 0;
    return std::clamp(std::hypot(double(point.x) - operation.start.x,
                                 double(point.y) - operation.start.y) / radius, 0.0, 1.0);
}

double normalizedOpacity(double opacity)
{
    return std::isfinite(opacity) ? std::clamp(opacity, 0.0, 1.0) : 0;
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
    if (surface.width <= 0 || surface.height <= 0) return;
    const auto width = static_cast<std::size_t>(surface.width);
    const auto count = std::min(surface.pixels.size(), width * static_cast<std::size_t>(surface.height));
    for (std::size_t index = 0; index < count; ++index) {
        const DevicePixelPoint point{static_cast<Types::Pixel>(index % width),
                                     static_cast<Types::Pixel>(index / width)};
        const double coverage = selectionAlphaAt(selection, point);
        if (coverage > 0) callback(point, index, coverage);
    }
}

} // namespace

void applyFill(DrawingSurface &surface, const SelectionState &selection, const FillOperation &operation)
{
    const double opacity = normalizedOpacity(operation.opacity);
    if (!operation.enabled || opacity == 0 || channel(operation.argb, 24U) == 0) {
        return;
    }

    forSelectedPixels(surface, selection, [&](DevicePixelPoint, std::size_t index, double coverage) {
        surface.pixels[index] = compositeRasterPixel(operation.argb, surface.pixels[index],
                                                     RasterBlendMode::SourceOver, opacity * coverage);
    });
}

void applyGradient(DrawingSurface &surface, const SelectionState &selection, const GradientOperation &operation)
{
    const double opacity = normalizedOpacity(operation.opacity);
    if (!operation.enabled || opacity == 0
            || !std::isfinite(operation.start.x) || !std::isfinite(operation.start.y)
            || !std::isfinite(operation.end.x) || !std::isfinite(operation.end.y)
            || !std::isfinite(std::hypot(operation.end.x - operation.start.x,
                                        operation.end.y - operation.start.y))) {
        return;
    }

    forSelectedPixels(surface, selection, [&](DevicePixelPoint point, std::size_t index, double coverage) {
        const double t = operation.kind == GradientKind::Linear
                ? linearGradientT(operation, point) : radialGradientT(operation, point);
        const auto source = mixArgb(operation.startArgb, operation.endArgb, t);
        if (channel(source, 24U) != 0)
            surface.pixels[index] = compositeRasterPixel(source, surface.pixels[index],
                                                         RasterBlendMode::SourceOver, opacity * coverage);
    });
}

void applyEraser(DrawingSurface &surface, const SelectionState &selection, const EraserOperation &operation)
{
    const double opacity = normalizedOpacity(operation.opacity);
    if (!operation.enabled || opacity == 0) {
        return;
    }

    forSelectedPixels(surface, selection, [&](DevicePixelPoint, std::size_t index, double coverage) {
        surface.pixels[index] = erasedArgb(surface.pixels[index], opacity * coverage);
    });
}
