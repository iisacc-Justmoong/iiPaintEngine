#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Brush/BrushPresetSerializer.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"
#include "tests/RasterDabTestUtils.h"

namespace {

std::uint8_t redOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 16U) & 0xFFU);
}

std::uint8_t blueOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>(argb & 0xFFU);
}

const RasterSample *sampleAt(const std::vector<RasterSample> &samples, DevicePixelPoint position)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y) {
            return &sample;
        }
    }
    return nullptr;
}

std::vector<BrushDab> horizontalDabs(const BrushState &brush)
{
    return streamTestDabs(brush,
                          StrokePoint{{0.0, 0.0}, 1.0, 0.0, 1.0, 0.0, 0.0, 1, 0.0},
                          StrokePoint{{12.0, 0.0}, 1.0, 1.0, 1.0, 0.0, 0.0, 1, 12.0});
}

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

std::uint32_t sampleRasterLayerArgb(const void *context, DevicePixelPoint position)
{
    const auto *layer = static_cast<const RasterLayer *>(context);
    if (layer == nullptr) {
        return 0x00000000U;
    }

    return rasterLayerPixelAt(*layer, position);
}

RasterSourceSampler sourceSamplerForLayer(const RasterLayer &layer, DevicePixelPoint origin = {})
{
    RasterSourceSampler sampler;
    sampler.context = &layer;
    sampler.sampleArgb = &sampleRasterLayerArgb;
    sampler.origin = origin;
    sampler.width = layer.width;
    sampler.height = layer.height;
    return sampler;
}

} // namespace

int main()
{
    RasterLayer source = makeRasterLayer(8, 3);
    source.pixels[static_cast<std::size_t>(1) * static_cast<std::size_t>(source.width) + 2] = 0xFF0000FFU;
    source.pixels[static_cast<std::size_t>(1) * static_cast<std::size_t>(source.width) + 3] = 0xFF0000FFU;

    Rasterizer rasterizer{};
    rasterizer.argb = 0xFFFF0000U;
    rasterizer.radius = 0;
    rasterizer.brushSize = 2.0;
    rasterizer.flow = 1.0;
    rasterizer.opacity = 1.0;

    BrushDab dab{};
    dab.position = {3.0, 1.0};
    dab.scale = 1.0;
    dab.alpha = 1.0;
    dab.opacityCapScale = 1.0;
    dab.colorArgb = 0xFFFF0000U;

    BrushMaterial smudgeMaterial;
    smudgeMaterial.simulation.enabled = true;
    smudgeMaterial.simulation.model = BrushSimulationModel::Smudge;
    smudgeMaterial.simulation.wetness = 1.0;
    smudgeMaterial.simulation.smudgeStrength = 1.0;
    smudgeMaterial.simulation.pickup = 1.0;
    smudgeMaterial.simulation.deposit = 0.0;
    const RasterSourceSampler sourceSampler = sourceSamplerForLayer(source);
    const std::vector<RasterSample> smudgeSamples = projectBrushDabs({dab},
                                                                     rasterizer,
                                                                     RasterProjection{},
                                                                     sourceSampler,
                                                                     smudgeMaterial);
    const RasterSample *smudged = sampleAt(smudgeSamples, {3, 1});
    if (smudged == nullptr || blueOf(smudged->argb) <= redOf(smudged->argb)) {
        return 10;
    }

    RasterLayer croppedSource = makeRasterLayer(2, 1);
    croppedSource.pixels[1] = 0xFF0000FFU;
    const RasterSourceSampler croppedSourceSampler = sourceSamplerForLayer(croppedSource, {2, 1});
    const std::vector<RasterSample> croppedSmudgeSamples = projectBrushDabs({dab},
                                                                            rasterizer,
                                                                            RasterProjection{},
                                                                            croppedSourceSampler,
                                                                            smudgeMaterial);
    const RasterSample *croppedSmudged = sampleAt(croppedSmudgeSamples, {3, 1});
    if (croppedSmudged == nullptr || blueOf(croppedSmudged->argb) <= redOf(croppedSmudged->argb)) {
        return 15;
    }

    BrushMaterial mixerMaterial;
    mixerMaterial.simulation.enabled = true;
    mixerMaterial.simulation.model = BrushSimulationModel::Mixer;
    mixerMaterial.simulation.wetness = 1.0;
    mixerMaterial.simulation.smudgeStrength = 0.0;
    mixerMaterial.simulation.pickup = 1.0;
    mixerMaterial.simulation.deposit = 0.5;
    mixerMaterial.simulation.mixStrength = 1.0;
    const std::vector<RasterSample> mixedSamples = projectBrushDabs({dab},
                                                                    rasterizer,
                                                                    RasterProjection{},
                                                                    sourceSampler,
                                                                    mixerMaterial);
    const RasterSample *mixed = sampleAt(mixedSamples, {3, 1});
    if (mixed == nullptr || redOf(mixed->argb) == 0 || blueOf(mixed->argb) == 0) {
        return 20;
    }

    BrushState roundBrush;
    roundBrush.rasterizer.brushSize = 4.0;
    roundBrush.rasterizer.spacingRatio = 0.5;
    const std::vector<BrushDab> roundDabs = horizontalDabs(roundBrush);

    BrushState flatBristleBrush = roundBrush;
    flatBristleBrush.material.bristle.enabled = true;
    flatBristleBrush.material.bristle.shape = BristleShape::Flat;
    flatBristleBrush.material.bristle.count = 16;
    flatBristleBrush.material.bristle.length = 6.0;
    flatBristleBrush.material.bristle.stiffness = 0.4;
    const std::vector<BrushDab> flatDabs = horizontalDabs(flatBristleBrush);
    if (roundDabs.empty()
            || flatDabs.empty()
            || !(flatDabs.front().ellipseScaleX > roundDabs.front().ellipseScaleX)
            || !(flatDabs.front().ellipseScaleY < roundDabs.front().ellipseScaleY)
            || !nearlyEqual(flatDabs.front().rotationRadians, 0.0)) {
        return 30;
    }

    BrushPreset preset;
    preset.material = mixerMaterial;
    const BrushPreset reopened = deserializeBrushPreset(serializeBrushPreset(preset));
    if (!nearlyEqual(reopened.material.simulation.pickup, 1.0)
            || !nearlyEqual(reopened.material.simulation.deposit, 0.5)) {
        return 40;
    }

    return 0;
}
