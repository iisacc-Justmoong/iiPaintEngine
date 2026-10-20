//
// Created by Justmoong on 2026 May 26.
//

#include "FilterPipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

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

std::uint32_t packArgb(int alpha, int red, int green, int blue)
{
    return (static_cast<std::uint32_t>(std::clamp(alpha, 0, 255)) << 24U)
            | (static_cast<std::uint32_t>(std::clamp(red, 0, 255)) << 16U)
            | (static_cast<std::uint32_t>(std::clamp(green, 0, 255)) << 8U)
            | static_cast<std::uint32_t>(std::clamp(blue, 0, 255));
}

std::uint8_t mixByte(std::uint8_t lhs, std::uint8_t rhs, Types::Scalar t)
{
    return static_cast<std::uint8_t>(std::clamp<int>(
            static_cast<int>(std::lround(static_cast<Types::Scalar>(lhs)
                                         + (static_cast<Types::Scalar>(rhs) - static_cast<Types::Scalar>(lhs))
                                                 * std::clamp(t, 0.0, 1.0))),
            0,
            255));
}

std::uint32_t mixArgb(std::uint32_t lhs, std::uint32_t rhs, Types::Scalar t)
{
    return (static_cast<std::uint32_t>(mixByte(channel(lhs, 24U), channel(rhs, 24U), t)) << 24U)
            | (static_cast<std::uint32_t>(mixByte(channel(lhs, 16U), channel(rhs, 16U), t)) << 16U)
            | (static_cast<std::uint32_t>(mixByte(channel(lhs, 8U), channel(rhs, 8U), t)) << 8U)
            | static_cast<std::uint32_t>(mixByte(channel(lhs, 0U), channel(rhs, 0U), t));
}

void applyBlur(DrawingSurface &surface, const SelectionState &selection, Types::Scalar radius)
{
    if (surface.width <= 0 || surface.height <= 0) return;
    const std::vector<std::uint32_t> source = surface.pixels;
    // A larger window covers the whole image. Clamp before integer conversion.
    const auto integerRadius = static_cast<std::int64_t>(std::min(
            static_cast<double>(std::max(surface.width, surface.height)),
            std::max(1.0, std::round(radius))));
    using Sum = std::array<std::uint64_t, 4>;
    std::vector<Sum> columns(static_cast<std::size_t>(surface.width));
    auto updateRow = [&](Types::Pixel row, bool add) {
        const auto start = static_cast<std::size_t>(row) * static_cast<std::size_t>(surface.width);
        for (Types::Pixel x = 0; x < surface.width; ++x) {
            const auto pixel = source[start + static_cast<std::size_t>(x)];
            for (unsigned c = 0; c < 4; ++c) {
                const auto value = channel(pixel, c * 8);
                if (add) columns[x][c] += value;
                else columns[x][c] -= value;
            }
        }
    };
    for (Types::Pixel y = 0; y <= std::min<std::int64_t>(integerRadius, surface.height - 1); ++y)
        updateRow(y, true);
    for (Types::Pixel y = 0; y < surface.height; ++y) {
        Sum sum{};
        for (Types::Pixel x = 0; x <= std::min<std::int64_t>(integerRadius, surface.width - 1); ++x)
            for (unsigned c = 0; c < 4; ++c) sum[c] += columns[x][c];
        const auto rows = std::min<std::int64_t>(surface.height - 1, y + integerRadius)
                - std::max<std::int64_t>(0, y - integerRadius) + 1;
        for (Types::Pixel x = 0; x < surface.width; ++x) {
            const DevicePixelPoint point{x, y};
            if (!selection.active || selectionAlphaAt(selection, point) > 0.0) {
                const auto count = static_cast<std::uint64_t>(rows)
                        * static_cast<std::uint64_t>(std::min<std::int64_t>(surface.width - 1, x + integerRadius)
                                                    - std::max<std::int64_t>(0, x - integerRadius) + 1);
                surface.pixels[surfaceIndex(surface, point)] = packArgb(
                        static_cast<int>(sum[3] / count), static_cast<int>(sum[2] / count),
                        static_cast<int>(sum[1] / count), static_cast<int>(sum[0] / count));
            }
            if (x + integerRadius + 1 < surface.width)
                for (unsigned c = 0; c < 4; ++c) sum[c] += columns[x + integerRadius + 1][c];
            if (x >= integerRadius)
                for (unsigned c = 0; c < 4; ++c) sum[c] -= columns[x - integerRadius][c];
        }
        if (y + integerRadius + 1 < surface.height) updateRow(static_cast<Types::Pixel>(y + integerRadius + 1), true);
        if (y >= integerRadius) updateRow(static_cast<Types::Pixel>(y - integerRadius), false);
    }
}

void applySmudge(DrawingSurface &surface, const SelectionState &selection, Types::Scalar strength)
{
    const std::vector<std::uint32_t> source = surface.pixels;
    for (Types::Pixel y = 0; y < surface.height; ++y) {
        for (Types::Pixel x = 1; x < surface.width; ++x) {
            const DevicePixelPoint point{x, y};
            if (selectionAlphaAt(selection, point) <= 0.0) {
                continue;
            }
            const DevicePixelPoint previous{x - 1, y};
            surface.pixels[surfaceIndex(surface, point)] = mixArgb(source[surfaceIndex(surface, point)],
                                                                   source[surfaceIndex(surface, previous)],
                                                                   strength);
        }
    }
}

} // namespace

void applyFilterPipeline(DrawingSurface &surface,
                         const SelectionState &selection,
                         const FilterPipeline &pipeline)
{
    for (const FilterNode &node : pipeline.nodes) {
        if (!node.enabled) {
            continue;
        }
        if (node.kind == FilterKind::Blur) {
            applyBlur(surface, selection, node.radius);
        } else if (node.kind == FilterKind::Smudge) {
            applySmudge(surface, selection, node.strength);
        }
    }
}
