#include <cmath>
#include <cstdint>
#include <vector>

#include "tests/RasterDabTestUtils.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

std::uint8_t alphaAt(const std::vector<RasterSample> &samples, DevicePixelPoint position)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y) {
            return alphaOf(sample.argb);
        }
    }

    return 0;
}

std::vector<BrushDab> pressureDabs(const BrushState &brush, Types::Scalar pressure)
{
    return streamTestDabs(brush,
            StrokePoint{{0.0, 0.0}, pressure, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{10.0, 0.0}, pressure, 1.0, 0.0, 0.0, 0.0, 1});
}

} // namespace

int main()
{
    BrushState brush{};
    brush.randomSeed = 44;
    brush.rasterizer.brushSize = 8.0;
    brush.rasterizer.spacingRatio = 0.25;
    brush.rasterizer.flow = 0.5;
    brush.rasterizer.opacity = 0.8;
    brush.dynamics.pressureToSize = 0.75;
    brush.dynamics.pressureToFlow = 0.5;
    brush.dynamics.pressureToOpacity = 0.5;
    brush.dynamics.velocityToSpacing = 0.05;
    brush.dynamics.velocityToOpacity = 0.02;
    brush.dynamics.velocityToDryOut = 0.06;
    brush.dynamics.tiltToRotation = true;
    brush.dynamics.tiltToEllipse = 0.5;
    brush.dynamics.tiltToTextureDirection = true;
    brush.dynamics.rotationJitter = 0.2;
    brush.dynamics.grainJitter = 0.4;

    const std::vector<BrushDab> command = streamTestDabs(
            brush,
            StrokePoint{{0.0, 0.0}, 0.2, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{10.0, 0.0}, 0.8, 1.0, 0.0, 0.0, 1.0, 1});
    if (command.size() < 3) {
        return 1;
    }

    const BrushDab &first = command.front();
    const BrushDab &last = command.back();

    if (!(last.scale > first.scale)) {
        return 1;
    }

    if (!(last.alpha < brush.rasterizer.flow)
            || !(last.opacityCapScale < 1.0)
            || !nearlyEqual(last.hardnessScale, 1.0)) {
        return 1;
    }

    if (!(last.ellipseScaleX > 1.0)
            || !(last.ellipseScaleY < 1.0)
            || !nearlyEqual(last.textureDirectionRadians, std::atan2(1.0, 0.0))) {
        return 1;
    }

    const std::vector<BrushDab> sameSeed = streamTestDabs(
            brush,
            StrokePoint{{0.0, 0.0}, 0.2, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{10.0, 0.0}, 0.8, 1.0, 0.0, 0.0, 1.0, 1});
    brush.randomSeed += 1;
    const std::vector<BrushDab> differentSeed = streamTestDabs(
            brush,
            StrokePoint{{0.0, 0.0}, 0.2, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{10.0, 0.0}, 0.8, 1.0, 0.0, 0.0, 1.0, 1});

    if (!nearlyEqual(command[1].rotationRadians, sameSeed[1].rotationRadians)
            || !nearlyEqual(command[1].grain, sameSeed[1].grain)
            || nearlyEqual(command[1].rotationRadians, differentSeed[1].rotationRadians)
            || nearlyEqual(command[1].grain, differentSeed[1].grain)) {
        return 1;
    }

    BrushState pressureBrush;
    pressureBrush.rasterizer.brushSize = 10.0;
    pressureBrush.rasterizer.spacingRatio = 0.25;
    pressureBrush.rasterizer.flow = 1.0;
    pressureBrush.rasterizer.opacity = 1.0;
    pressureBrush.rasterizer.hardness = 1.0;
    pressureBrush.dynamics.pressureToSize = 1.0;
    pressureBrush.dynamics.pressureToFlow = 1.0;
    pressureBrush.dynamics.pressureToOpacity = 1.0;

    const std::vector<BrushDab> lowPressure = pressureDabs(pressureBrush, 0.0);
    const std::vector<BrushDab> highPressure = pressureDabs(pressureBrush, 1.0);
    if (lowPressure.size() < 2
            || highPressure.size() != lowPressure.size()) {
        return 1;
    }

    for (std::size_t index = 0; index < lowPressure.size(); ++index) {
        if (!nearlyEqual(lowPressure[index].position.x, highPressure[index].position.x)
                || !nearlyEqual(lowPressure[index].position.y, highPressure[index].position.y)) {
            return 1;
        }
    }

    if (!(highPressure.front().scale > lowPressure.front().scale)
            || !(highPressure.front().alpha > lowPressure.front().alpha)
            || !(highPressure.front().opacityCapScale > lowPressure.front().opacityCapScale)
            || !nearlyEqual(highPressure.front().hardnessScale, lowPressure.front().hardnessScale)
            || !nearlyEqual(highPressure.front().alpha, 1.0)
            || !nearlyEqual(highPressure.front().opacityCapScale, 1.0)
            || !nearlyEqual(highPressure.front().hardnessScale, 1.0)) {
        return 1;
    }

    Rasterizer hardnessRasterizer;
    hardnessRasterizer.argb = 0xFFFFFFFFU;
    hardnessRasterizer.brushWidth = 2;
    hardnessRasterizer.brushHeight = 2;
    hardnessRasterizer.brushAlpha = {
            255, 0,
            0, 0,
    };
    hardnessRasterizer.hardness = 1.0;
    BrushDab softDab;
    softDab.position = {20.0, 20.0};
    softDab.scale = 2.0;
    softDab.alpha = 1.0;
    softDab.hardnessScale = 0.2;
    softDab.colorArgb = 0xFFFFFFFFU;
    BrushDab hardDab = softDab;
    hardDab.hardnessScale = 1.0;
    const std::vector<RasterSample> softSamples = projectBrushDabs({softDab}, hardnessRasterizer);
    const std::vector<RasterSample> hardSamples = projectBrushDabs({hardDab}, hardnessRasterizer);
    if (alphaAt(softSamples, {20, 20}) >= alphaAt(hardSamples, {20, 20})
            || alphaAt(hardSamples, {20, 20}) == 0) {
        return 1;
    }

    return 0;
}
