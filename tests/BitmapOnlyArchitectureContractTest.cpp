#include <array>
#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>

#include "Document/PaintDocument.h"
#include "Input/InputStrokeBuilder.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

namespace {

template <typename, typename = void>
struct HasPointCollection : std::false_type {
};

template <typename T>
struct HasPointCollection<T, std::void_t<decltype(std::declval<T>().points)>> : std::true_type {
};

template <typename, typename = void>
struct HasRetainedStrokes : std::false_type {
};

template <typename T>
struct HasRetainedStrokes<T, std::void_t<decltype(std::declval<T>().strokes)>> : std::true_type {
};

PointerEvent mouseEvent(PointerEventPhase phase,
                        DocumentPoint position,
                        Types::Scalar time,
                        PointerButton button,
                        bool down)
{
    return PointerEvent{
            PointerDeviceKind::Mouse,
            phase,
            position,
            1.0,
            time,
            button,
            down,
    };
}

} // namespace

int main()
{
    static_assert(!HasPointCollection<InputStrokeBuilder>::value,
                  "Pointer input may not retain a coordinate collection.");
    static_assert(!HasPointCollection<InputStrokeBuildResult>::value,
                  "Pointer results may expose only the current bitmap input point.");
    static_assert(!HasPointCollection<RasterDabStream>::value,
                  "Raster spacing state may retain only one previous point.");
    static_assert(!HasRetainedStrokes<PaintDocument>::value,
                  "A bitmap document may not retain replayable strokes.");

    const std::filesystem::path sourceRoot{IIPAINTENGINE_SOURCE_DIR};
    constexpr std::array<const char *, 12> forbiddenSourceStems{
            "Layer/StrokeLayer",
            "Layer/TextLayer",
            "Layer/VectorLayer",
            "Stroke/LiveStroke",
            "Stroke/Stabilizer",
            "Stroke/Stroke",
            "Stroke/StrokeCommand",
            "Stroke/StrokeCurve",
            "Stroke/StrokeGeometry",
            "Stroke/StrokeInput",
            "Stroke/StrokeRepository",
            "Stroke/StrokeResampler",
    };
    for (const char *relativeStem : forbiddenSourceStems) {
        if (std::filesystem::exists(sourceRoot / (std::string{relativeStem} + ".h"))
                || std::filesystem::exists(sourceRoot / (std::string{relativeStem} + ".cpp"))) {
            return 1;
        }
    }

    InputStrokeBuilder input{};
    BrushState brush{};
    brush.rasterizer.radius = 0;
    brush.rasterizer.spacing = 1.0;
    brush.rasterizer.argb = 0xFF336699U;
    RasterDabStream stream{};
    StrokeCompositeBuffer pixels = makeStrokeCompositeBuffer(8, 4);

    const InputStrokeBuildResult press = appendPointerEvent(
            input,
            mouseEvent(PointerEventPhase::Press, {1.0, 1.0}, 0.0, PointerButton::Primary, true));
    if (!press.strokeStarted || !press.pointAvailable || !input.active) {
        return 1;
    }
    accumulateStrokeSamples(pixels,
                            projectBrushDabs(appendRasterDabs(stream, press.point, brush), brush.rasterizer));

    const InputStrokeBuildResult move = appendPointerEvent(
            input,
            mouseEvent(PointerEventPhase::Move, {3.0, 1.0}, 1.0, PointerButton::None, true));
    if (!move.pointAvailable || move.strokeCompleted) {
        return 1;
    }
    accumulateStrokeSamples(pixels,
                            projectBrushDabs(appendRasterDabs(stream, move.point, brush), brush.rasterizer));

    const InputStrokeBuildResult release = appendPointerEvent(
            input,
            mouseEvent(PointerEventPhase::Release, {5.0, 1.0}, 2.0, PointerButton::Primary, false));
    if (!release.pointAvailable || !release.strokeCompleted || input.active) {
        return 1;
    }
    accumulateStrokeSamples(pixels,
                            projectBrushDabs(appendRasterDabs(stream, release.point, brush, true),
                                             brush.rasterizer));

    RasterLayer layer = makeRasterLayer(8, 4);
    compositeStrokeBufferOntoLayer(layer, pixels);
    return rasterLayerPixelAt(layer, {1, 1}) == 0xFF336699U
            && rasterLayerPixelAt(layer, {3, 1}) == 0xFF336699U
            && rasterLayerPixelAt(layer, {5, 1}) == 0xFF336699U
            ? 0
            : 1;
}
