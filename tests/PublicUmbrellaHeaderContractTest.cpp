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
    static_assert(isCompleteType<BitmapPaintSession>());
    static_assert(isCompleteType<BitmapPaintUpdate>());
    static_assert(isCompleteType<BitmapFile>());
    static_assert(isCompleteType<BitmapFileWriteOptions>());
    static_assert(isCompleteType<PixelMask>());
    static_assert(isCompleteType<LocalMaskBrushOptions>());
    static_assert(isCompleteType<MaskPaintSession>());
    static_assert(isCompleteType<MaskPaintUpdate>());

    static_assert(__cplusplus > 202002L);
    BitmapPaintSession session;
    MaskPaintSession mask;
    return !session.file().isOpen() && !session.strokeActive() && session.brushPreset().size > 0
            && !mask.hasMask() && mask.createMask(4,4) && mask.mask().coverage.size() == 16 ? 0 : 1;
}
