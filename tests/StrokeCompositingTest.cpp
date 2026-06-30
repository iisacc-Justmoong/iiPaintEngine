#include <cstdint>
#include <vector>

#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

std::uint8_t redOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 16U) & 0xFFU);
}

std::uint8_t blueOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>(argb & 0xFFU);
}

} // namespace

int main()
{
    StrokeCompositeBuffer buffer = makeStrokeCompositeBuffer(3, 3);
    const std::vector<RasterSample> repeatedRed{
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0x40FF0000U, 0x80U, RasterBlendMode::SourceOver},
    };

    accumulateStrokeSamples(buffer, repeatedRed);
    if (alphaOf(strokeCompositePixelAt(buffer, {1, 1})) != 0x80) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(3, 3, 0xFF0000FFU);
    compositeStrokeBufferOntoLayer(layer, buffer);
    const std::uint32_t composited = rasterLayerPixelAt(layer, {1, 1});
    if (alphaOf(composited) != 0xFF || redOf(composited) < 0x7F || blueOf(composited) > 0x80) {
        return 1;
    }

    RasterLayer directLayer = makeRasterLayer(3, 3, 0xFF0000FFU);
    paintRasterSamples(directLayer, repeatedRed);
    if (rasterLayerPixelAt(directLayer, {1, 1}) != composited) {
        return 1;
    }

    RasterLayer erasedLayer = makeRasterLayer(3, 3, 0xFF336699U);
    paintRasterSamples(erasedLayer, {
            RasterSample{{1, 1}, 0x80336699U, 0xFFU, RasterBlendMode::DestinationOut},
            RasterSample{{2, 2}, 0xFF336699U, 0xFFU, RasterBlendMode::DestinationOut},
    });
    if (alphaOf(rasterLayerPixelAt(erasedLayer, {1, 1})) >= 0x90
            || rasterLayerPixelAt(erasedLayer, {2, 2}) != 0x00000000U
            || rasterLayerPixelAt(erasedLayer, {0, 0}) != 0xFF336699U) {
        return 1;
    }

    RasterLayer mixedLayer = makeRasterLayer(3, 3, 0xFF0000FFU);
    paintRasterSamples(mixedLayer, {
            RasterSample{{0, 0}, 0x80FF0000U, 0xFFU, RasterBlendMode::SourceOver},
            RasterSample{{1, 1}, 0xFF000000U, 0xFFU, RasterBlendMode::DestinationOut},
            RasterSample{{2, 2}, 0x80FF0000U, 0xFFU, RasterBlendMode::SourceOver},
    });
    if (redOf(rasterLayerPixelAt(mixedLayer, {0, 0})) <= 0x7F
            || rasterLayerPixelAt(mixedLayer, {1, 1}) != 0x00000000U
            || redOf(rasterLayerPixelAt(mixedLayer, {2, 2})) <= 0x7F) {
        return 1;
    }

    Rasterizer eraserRasterizer{};
    eraserRasterizer.radius = 1;
    eraserRasterizer.argb = 0xFF000000U;
    eraserRasterizer.blendMode = RasterBlendMode::DestinationOut;
    BrushDab eraserDab{};
    eraserDab.position = {1.0, 1.0};
    eraserDab.colorArgb = 0xFF000000U;
    eraserDab.blendMode = RasterBlendMode::DestinationOut;
    const std::vector<RasterSample> eraserSamples = projectBrushDabs({eraserDab}, eraserRasterizer);
    if (eraserSamples.empty()) {
        return 1;
    }
    for (const RasterSample &sample : eraserSamples) {
        if (sample.blendMode != RasterBlendMode::DestinationOut) {
            return 1;
        }
    }

    if (rasterLayerPixelAt(layer, {0, 0}) != 0xFF0000FFU) {
        return 1;
    }

    return 0;
}
