#include "FrozenRender.h"
#include <new>
#include <stdexcept>
#include <utility>

namespace ii::brush {
namespace {
template <class Render>
std::expected<RenderedStroke,FrozenRenderError> withFrozenSource(
    DevicePixelRect clip,const FrozenSource& source,std::uint32_t argb,std::uint32_t seed,
    const DynamicsContext& dynamics,std::size_t maxMissingTiles,Render&& render) {
    FrozenSourceSampler reader(source,maxMissingTiles);
    const RenderTarget target{clip,reader.sampler(),argb,seed,dynamics};
    auto rendered=std::forward<Render>(render)(target);
    if (!reader.complete()) {
        if (reader.reportAllocationFailure())
            return std::unexpected(FrozenRenderError{FrozenRenderErrorCode::Allocation,{},{}});
        if (!reader.reportComplete())
            return std::unexpected(FrozenRenderError{FrozenRenderErrorCode::MissingReportLimit,{},{}});
        try {
            return std::unexpected(FrozenRenderError{FrozenRenderErrorCode::MissingSource,{},reader.missingTiles()});
        } catch (const std::bad_alloc&) {
            return std::unexpected(FrozenRenderError{FrozenRenderErrorCode::Allocation,{},{}});
        } catch (const std::length_error&) {
            return std::unexpected(FrozenRenderError{FrozenRenderErrorCode::Allocation,{},{}});
        }
    }
    if (!rendered)
        return std::unexpected(FrozenRenderError{FrozenRenderErrorCode::BrushFailure,rendered.error(),{}});
    return std::move(*rendered);
}
} // namespace
std::expected<RenderedStroke,FrozenRenderError> renderFrozenGesture(
    std::span<const StrokePoint> inputs,Tool tool,const Controls& controls,
    DevicePixelRect clip,const FrozenSource& source,std::uint32_t argb,
    std::uint32_t seed,const DynamicsContext& dynamics,const Limits& limits,
    std::size_t maxMissingTiles) {
    return withFrozenSource(clip,source,argb,seed,dynamics,maxMissingTiles,
        [&](const RenderTarget& target) { return renderGesture(inputs,tool,controls,target,limits); });
}
std::expected<RenderedStroke,FrozenRenderError> renderFrozenGestureWithPreset(
    std::span<const StrokePoint> inputs,Tool tool,const Controls& controls,const BrushPreset& base,
    DevicePixelRect clip,const FrozenSource& source,std::uint32_t argb,std::uint32_t seed,
    const DynamicsContext& dynamics,const Limits& limits,std::size_t maxMissingTiles) {
    return withFrozenSource(clip,source,argb,seed,dynamics,maxMissingTiles,
        [&](const RenderTarget& target) { return renderGestureWithPreset(inputs,tool,controls,base,target,limits); });
}
} // namespace ii::brush
