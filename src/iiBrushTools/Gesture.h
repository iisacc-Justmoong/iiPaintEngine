#pragma once

#include "Controls.h"
#include <Layer/RasterLayer.h>
#include <Stroke/Rasterizer.h>
#include <vector>

namespace ii::brush {
struct Limits {
    std::size_t maxInputPoints = 65536;
    std::size_t maxDabs = 262144;
    std::size_t maxProjectedPixels = 16777216;
    std::size_t maxTouchedPixels = 4194304;
};
// Transient authoring values. Never insert these samples into a document/history.
struct PreparedGesture {
    std::vector<StrokePoint> points;
    std::vector<double> distances;
    double length = 0;
};
[[nodiscard]] std::expected<PreparedGesture, Error> prepareGesture(std::span<const StrokePoint> inputs,
    const Controls& controls, const Limits& limits = {});
struct RenderTarget {
    DevicePixelRect clip{}; // Document pixel coordinates at scale 1.
    RasterSourceSampler source{}; // Immutable stroke-start pixels; empty means transparent.
    std::uint32_t argb = 0xff000000U;
    std::uint32_t seed = 1;
    DynamicsContext dynamics{};
};
struct PixelTile {
    DevicePixelRect bounds{}; // Clipped world bounds; pixels use local coordinates.
    RasterLayer pixels;
};
struct RenderedStroke {
    std::vector<PixelTile> tiles; // Final ARGB pixels, ordered by row then column.
    DevicePixelRect dirty{};
    double length = 0;
    std::size_t dabCount = 0;
};
// Synchronous authoring render. Input and sampler are borrowed for this call only.
// Rejection returns no partial tile result and never writes to the source.
[[nodiscard]] std::expected<RenderedStroke, Error> renderGesture(std::span<const StrokePoint> inputs,
    Tool tool, const Controls& controls, const RenderTarget& target, const Limits& limits = {});
[[nodiscard]] std::expected<RenderedStroke, Error> renderGestureWithPreset(std::span<const StrokePoint> inputs,
    Tool tool, const Controls& controls, const BrushPreset& base, const RenderTarget& target,
    const Limits& limits = {});
} // namespace ii::brush
