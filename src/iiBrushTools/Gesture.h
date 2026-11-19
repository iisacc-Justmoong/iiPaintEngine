#pragma once

#include "Controls.h"
#include "PreviewQueue.h"
#include <Layer/RasterLayer.h>
#include <Stroke/Rasterizer.h>
#include <cstdint>
#include <memory>
#include <span>
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
    // source.context의 불변 ARGB를 source-local 시작 좌표부터 대상 span에 복사한다.
    // span 전체가 원본 안에 있으며, 개별 위치를 읽는 sampleArgb도 함께 제공한다.
    using CopySourceRow = void (*)(const void* context, DevicePixelPoint start, std::span<std::uint32_t> destination);
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
// 타일 초기 픽셀을 행 단위로 읽는 전체 획 렌더이다.
[[nodiscard]] std::expected<RenderedStroke, Error> renderGesture(std::span<const StrokePoint> inputs,
    Tool tool, const Controls& controls, const RenderTarget& target, const Limits& limits,
    RenderTarget::CopySourceRow copySourceRow);
// Render an already validated brush state with authored taper and input correction.
// The caller owns the state and must provide a supported source-over or eraser brush.
[[nodiscard]] std::expected<RenderedStroke, Error> renderGestureWithResolvedBrush(std::span<const StrokePoint> inputs,
    const ResolvedBrush& brush, const Controls& controls, const RenderTarget& target, const Limits& limits = {});
// 고정한 브러시와 행 단위 원본 복사를 결합한다.
[[nodiscard]] std::expected<RenderedStroke, Error> renderGestureWithResolvedBrush(std::span<const StrokePoint> inputs,
    const ResolvedBrush& brush, const Controls& controls, const RenderTarget& target, const Limits& limits,
    RenderTarget::CopySourceRow copySourceRow);
[[nodiscard]] std::expected<RenderedStroke, Error> renderGestureWithPreset(std::span<const StrokePoint> inputs,
    Tool tool, const Controls& controls, const BrushPreset& base, const RenderTarget& target,
    const Limits& limits = {});
// 프리셋을 해석한 뒤 행 단위 원본 복사를 사용한다.
[[nodiscard]] std::expected<RenderedStroke, Error> renderGestureWithPreset(std::span<const StrokePoint> inputs,
    Tool tool, const Controls& controls, const BrushPreset& base, const RenderTarget& target,
    const Limits& limits, RenderTarget::CopySourceRow copySourceRow);

struct GesturePreviewUpdate {
    RenderedStroke stroke; // Replacement tiles for this batch. Length and dab count are cumulative.
    bool replacesStroke = false; // Discard provisional tiles before applying the final authored result.
};

// Serial worker state for one PreviewQueue. The source sampler must keep the same immutable
// stroke-start pixels available through completion. Create a new renderer for a different source.
class GesturePreviewRenderer final {
public:
    GesturePreviewRenderer(ResolvedBrush brush, Controls controls, RenderTarget target, Limits limits = {});
    // 원본 콜백과 컨텍스트를 작업자 렌더가 끝날 때까지 유지한다.
    GesturePreviewRenderer(ResolvedBrush brush, Controls controls, RenderTarget target, Limits limits,
                           RenderTarget::CopySourceRow copySourceRow);
    ~GesturePreviewRenderer();

    // Accepts consecutive leases and keeps the checkpoint only on success. Apply successful
    // Advance deltas to staging, then expose that staging on Publish or Commit.
    [[nodiscard]] std::expected<GesturePreviewUpdate, Error> render(const PreviewQueue::Work& work);

    // Release a cancelled epoch's tile state on the same worker. Older cancellation cannot clear newer state.
    void cancel(std::uint64_t epoch) noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace ii::brush
