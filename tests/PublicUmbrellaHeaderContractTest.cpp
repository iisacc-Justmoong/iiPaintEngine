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
    static_assert(isCompleteType<RasterViewport>());
    static_assert(isCompleteType<Layer>());
    static_assert(isCompleteType<RasterLayer>());
    static_assert(isCompleteType<BrushPreset>());
    static_assert(isCompleteType<BrushDynamics>());
    static_assert(isCompleteType<StrokePoint>());
    static_assert(isCompleteType<RasterDabStream>());
    static_assert(isCompleteType<Renderer>());
    static_assert(isCompleteType<DirtyRegion>());
    static_assert(isCompleteType<HistoryStack>());
    static_assert(isCompleteType<PointerEvent>());
    static_assert(isCompleteType<PaintColor>());
    static_assert(isCompleteType<SelectionState>());
    static_assert(isCompleteType<TransformState>());
    static_assert(isCompleteType<FilterPipeline>());
    static_assert(isCompleteType<FillOperation>());
    static_assert(isCompleteType<CanvasAdapter>());
    static_assert(isCompleteType<CanvasBrushConfig>());
    static_assert(isCompleteType<CanvasViewportConfig>());
    static_assert(isCompleteType<CanvasRuntimeConfig>());
    static_assert(isCompleteType<CanvasStateSnapshot>());
    static_assert(isCompleteType<BitmapReadResult>());
    static_assert(isCompleteType<BitmapWriteOptions>());

    CanvasBrushConfig brush;
    CanvasViewportConfig viewport;
    CanvasRuntimeConfig runtime;
    CanvasStateSnapshot state;

    return brush.size > 0.0
            && brush.pressureToOpacityEnabled
            && viewport.zoom > 0.0
            && runtime.livePreviewEnabled
            && state.inputPressure >= 0.0
            ? 0
            : 1;
}
