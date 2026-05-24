#include <cstdint>

#include "Document/PaintDocument.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeInput.h"

int main()
{
    StrokeInput input{{
            StrokePoint{{4.0, 4.0}, 1.0, 0.0},
            StrokePoint{{5.0, 7.0}, 1.0, 1.0},
            StrokePoint{{6.0, 4.0}, 1.0, 2.0},
    }};

    Stabilizer stabilizer{0.5};
    StrokeInput stabilized = stabilizeStrokeInput(input, stabilizer);
    if (stabilized.points.size() != input.points.size()) {
        return 1;
    }

    if (stabilized.points.front().position.x != input.points.front().position.x
            || stabilized.points.back().position.x != input.points.back().position.x) {
        return 1;
    }

    if (stabilized.points[1].position.y >= input.points[1].position.y) {
        return 1;
    }

    StrokeCurve curve = makeStrokeCurve(stabilized);
    if (curve.points.size() != stabilized.points.size()) {
        return 1;
    }

    Rasterizer rasterizer{};
    auto samples = rasterizeStrokeCurve(curve, rasterizer);
    if (samples.empty()) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(16, 16);
    paintRasterSamples(layer, samples);

    constexpr std::uint32_t black = 0xFF000000U;
    if (rasterLayerPixelAt(layer, {5, 5}) != black) {
        return 1;
    }

    PaintDocument document = makePaintDocument(layer);
    if (document.rasterLayers.size() != 1) {
        return 1;
    }

    if (rasterLayerPixelAt(document.rasterLayers.front(), {5, 5}) != black) {
        return 1;
    }

    return 0;
}
