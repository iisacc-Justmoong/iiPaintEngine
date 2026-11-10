#pragma once

#include "FrozenSource.h"
#include <iiBrushTools/Gesture.h>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace ii::brush {
enum class FrozenRenderErrorCode { MissingSource, MissingReportLimit, Allocation, BrushFailure };
struct FrozenRenderError {
    FrozenRenderErrorCode code;
    std::optional<Error> brush;
    std::vector<DevicePixelRect> missingTiles;
};
// Rejects the whole render if any in-bounds source pixel was not captured.
// The caller still owns region planning and source-revision consistency.
[[nodiscard]] std::expected<RenderedStroke,FrozenRenderError> renderFrozenGesture(
    std::span<const StrokePoint> inputs,Tool tool,const Controls& controls,
    DevicePixelRect clip,const FrozenSource& source,std::uint32_t argb=0xff000000U,
    std::uint32_t seed=1,const DynamicsContext& dynamics={},const Limits& limits={},
    std::size_t maxMissingTiles=65536);
// Like renderGestureWithPreset, retains supported base-preset features while
// rejecting the entire render if any in-bounds source tile is missing.
[[nodiscard]] std::expected<RenderedStroke,FrozenRenderError> renderFrozenGestureWithPreset(
    std::span<const StrokePoint> inputs,Tool tool,const Controls& controls,const BrushPreset& base,
    DevicePixelRect clip,const FrozenSource& source,std::uint32_t argb=0xff000000U,
    std::uint32_t seed=1,const DynamicsContext& dynamics={},const Limits& limits={},
    std::size_t maxMissingTiles=65536);
} // namespace ii::brush
