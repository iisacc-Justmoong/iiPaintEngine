#include <iiPaintEngine>

#include <type_traits>
#include <utility>

namespace {

template <typename T>
constexpr bool isCompleteType()
{
    return requires { sizeof(T); };
}

} // namespace

int main()
{
    static_assert(isCompleteType<EngineConfig>());
    static_assert(isCompleteType<PaintDocument>());
    static_assert(isCompleteType<DocumentSnapshot>());
    static_assert(isCompleteType<Canvas>());
    static_assert(isCompleteType<CanvasSession>());
    static_assert(isCompleteType<Layer>());
    static_assert(isCompleteType<RasterLayer>());
    static_assert(isCompleteType<BrushPreset>());
    static_assert(isCompleteType<BrushDynamics>());
    static_assert(isCompleteType<StrokeInput>());
    static_assert(isCompleteType<Stroke>());
    static_assert(isCompleteType<StrokeGeometryReport>());
    static_assert(isCompleteType<Renderer>());
    static_assert(isCompleteType<DirtyRegion>());
    static_assert(isCompleteType<HistoryStack>());
    static_assert(isCompleteType<PointerEvent>());
    static_assert(isCompleteType<PaintColor>());
    static_assert(isCompleteType<Selection>());
    static_assert(isCompleteType<Transform>());
    static_assert(isCompleteType<FilterPipeline>());
    static_assert(isCompleteType<RasterEditTool>());
    static_assert(isCompleteType<CanvasAdapter>());
    static_assert(isCompleteType<CanvasBrushConfig>());
    static_assert(isCompleteType<CanvasViewportConfig>());
    static_assert(isCompleteType<CanvasRuntimeConfig>());
    static_assert(isCompleteType<CanvasStateSnapshot>());

    static_assert(std::is_same_v<
            decltype(describeStrokeGeometry(std::declval<const StrokeInput &>())),
            StrokeGeometryReport>);

    CanvasBrushConfig brush;
    CanvasViewportConfig viewport;
    CanvasRuntimeConfig runtime;
    CanvasStateSnapshot state;

    return brush.size > 0.0
            && viewport.zoom > 0.0
            && runtime.livePreviewFrameIntervalMs >= 0
            && state.inputPressure >= 0.0
            ? 0
            : 1;
}
