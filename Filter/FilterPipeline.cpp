//
// Created by Justmoong on 2026 May 26.
//

#include "FilterPipeline.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

std::size_t surfaceIndex(const DrawingSurface &surface, DevicePixelPoint point)
{
    return static_cast<std::size_t>(point.y) * static_cast<std::size_t>(surface.width)
            + static_cast<std::size_t>(point.x);
}

bool contains(const DrawingSurface &surface, DevicePixelPoint point)
{
    return point.x >= 0 && point.y >= 0 && point.x < surface.width && point.y < surface.height;
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
    const std::vector<std::uint32_t> source = surface.pixels;
    const int integerRadius = std::max(1, static_cast<int>(std::lround(radius)));
    for (Types::Pixel y = 0; y < surface.height; ++y) {
        for (Types::Pixel x = 0; x < surface.width; ++x) {
            const DevicePixelPoint point{x, y};
            if (selectionAlphaAt(selection, point) <= 0.0) {
                continue;
            }

            int count = 0;
            int alpha = 0;
            int red = 0;
            int green = 0;
            int blue = 0;
            for (int offsetY = -integerRadius; offsetY <= integerRadius; ++offsetY) {
                for (int offsetX = -integerRadius; offsetX <= integerRadius; ++offsetX) {
                    const DevicePixelPoint samplePoint{x + offsetX, y + offsetY};
                    if (!contains(surface, samplePoint)) {
                        continue;
                    }
                    const std::uint32_t sample = source[surfaceIndex(surface, samplePoint)];
                    alpha += channel(sample, 24U);
                    red += channel(sample, 16U);
                    green += channel(sample, 8U);
                    blue += channel(sample, 0U);
                    ++count;
                }
            }
            if (count > 0) {
                surface.pixels[surfaceIndex(surface, point)] = packArgb(alpha / count, red / count, green / count, blue / count);
            }
        }
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
        if (node.kind == FilterKind::Blur) {
            applyBlur(surface, selection, node.radius);
        } else if (node.kind == FilterKind::Smudge) {
            applySmudge(surface, selection, node.strength);
        }
    }
}
