#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Stroke/StrokeCommand.h"

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

std::uint8_t opacityCapAt(const std::vector<RasterSample> &samples, DevicePixelPoint position)
{
    for (const RasterSample &sample : samples) {
        if (sample.position.x == position.x && sample.position.y == position.y) {
            return sample.opacityCap;
        }
    }

    return 0;
}

StrokeInput expressiveInput()
{
    StrokeInput input;
    input.points.push_back(StrokePoint{{0.0, 0.0}, 0.2, 0.0, 0.0, 0.0, 1.0, 1});
    input.points.push_back(StrokePoint{{16.0, 0.0}, 0.8, 0.2, 0.0, 0.0, 1.0, 1});
    return input;
}

StrokeInput neutralPressureInput()
{
    StrokeInput input = expressiveInput();
    for (StrokePoint &point : input.points) {
        point.pressure = 1.0;
    }
    return input;
}

BrushState sensitiveBrush()
{
    BrushState brush;
    brush.randomSeed = 11;
    brush.rasterizer.brushSize = 8.0;
    brush.rasterizer.spacingRatio = 0.25;
    brush.rasterizer.pressureScale = 0.5;
    brush.rasterizer.rotationJitter = 0.25;
    brush.rasterizer.flow = 0.8;
    brush.dynamics.pressureToSize = 0.75;
    brush.dynamics.pressureToFlow = 0.5;
    brush.dynamics.pressureToOpacity = 0.5;
    brush.dynamics.velocityToSpacing = 0.1;
    brush.dynamics.velocityToOpacity = 0.02;
    brush.dynamics.velocityToDryOut = 0.02;
    brush.dynamics.tiltToRotation = true;
    brush.dynamics.tiltToEllipse = 0.5;
    brush.dynamics.tiltToTextureDirection = true;
    brush.dynamics.rotationJitter = 0.4;
    brush.dynamics.grainJitter = 0.6;
    return brush;
}

} // namespace

int main()
{
    BrushState pressureDisabled = sensitiveBrush();
    pressureDisabled.dynamics.pressureInputEnabled = false;
    const StrokeCommand withoutPressure = makeStrokeCommand(expressiveInput(), pressureDisabled, Stabilizer{0.0});
    const StrokeCommand neutralPressure = makeStrokeCommand(neutralPressureInput(), pressureDisabled, Stabilizer{0.0});
    if (withoutPressure.dabs.empty()
            || neutralPressure.dabs.empty()
            || !nearlyEqual(withoutPressure.dabs.front().scale, neutralPressure.dabs.front().scale)
            || !nearlyEqual(withoutPressure.dabs.back().alpha, neutralPressure.dabs.back().alpha)
            || !nearlyEqual(withoutPressure.dabs.back().opacityCapScale, neutralPressure.dabs.back().opacityCapScale)) {
        return 1;
    }

    BrushState velocityEnabled = sensitiveBrush();
    BrushState velocityDisabled = velocityEnabled;
    velocityDisabled.dynamics.velocityInputEnabled = false;
    const StrokeCommand fastVelocity = makeStrokeCommand(expressiveInput(), velocityEnabled, Stabilizer{0.0});
    const StrokeCommand ignoredVelocity = makeStrokeCommand(expressiveInput(), velocityDisabled, Stabilizer{0.0});
    if (fastVelocity.dabs.empty()
            || ignoredVelocity.dabs.empty()
            || ignoredVelocity.dabs.size() <= fastVelocity.dabs.size()
            || !(ignoredVelocity.dabs.back().alpha > fastVelocity.dabs.back().alpha)) {
        return 1;
    }

    BrushState tiltDisabled = sensitiveBrush();
    tiltDisabled.dynamics.tiltInputEnabled = false;
    tiltDisabled.dynamics.randomInputEnabled = false;
    const StrokeCommand withoutTilt = makeStrokeCommand(expressiveInput(), tiltDisabled, Stabilizer{0.0});
    if (withoutTilt.dabs.empty()
            || !nearlyEqual(withoutTilt.dabs.back().rotationRadians, 0.0)
            || !nearlyEqual(withoutTilt.dabs.back().ellipseScaleX, 1.0)
            || !nearlyEqual(withoutTilt.dabs.back().ellipseScaleY, 1.0)
            || !nearlyEqual(withoutTilt.dabs.back().textureDirectionRadians, 0.0)) {
        return 1;
    }

    BrushState randomDisabled = sensitiveBrush();
    randomDisabled.dynamics.randomInputEnabled = false;
    const StrokeCommand firstSeed = makeStrokeCommand(expressiveInput(), randomDisabled, Stabilizer{0.0});
    randomDisabled.randomSeed += 1;
    const StrokeCommand secondSeed = makeStrokeCommand(expressiveInput(), randomDisabled, Stabilizer{0.0});
    if (firstSeed.dabs.size() < 2
            || secondSeed.dabs.size() < 2
            || !nearlyEqual(firstSeed.dabs[1].rotationRadians, secondSeed.dabs[1].rotationRadians)
            || !nearlyEqual(firstSeed.dabs[1].grain, secondSeed.dabs[1].grain)) {
        return 1;
    }

    BrushState perMappingEnabled = sensitiveBrush();
    BrushState perMappingDisabled = perMappingEnabled;
    perMappingDisabled.dynamics.pressureToSizeEnabled = false;
    perMappingDisabled.dynamics.velocityToDryOutEnabled = false;
    perMappingDisabled.dynamics.tiltToEllipseEnabled = false;
    perMappingDisabled.dynamics.rotationJitterEnabled = false;
    perMappingDisabled.dynamics.grainJitterEnabled = false;
    const StrokeCommand mappingEnabled = makeStrokeCommand(expressiveInput(), perMappingEnabled, Stabilizer{0.0});
    const StrokeCommand partlyDisabled = makeStrokeCommand(expressiveInput(), perMappingDisabled, Stabilizer{0.0});
    const Types::Scalar expectedDirectPressureScale = 1.0
            + (expressiveInput().points.back().pressure - 1.0) * perMappingDisabled.rasterizer.pressureScale;
    if (partlyDisabled.dabs.size() < 2
            || mappingEnabled.dabs.size() < 2
            || !nearlyEqual(partlyDisabled.dabs.back().scale, expectedDirectPressureScale)
            || !(partlyDisabled.dabs.back().scale > mappingEnabled.dabs.back().scale)
            || !(partlyDisabled.dabs.back().alpha > mappingEnabled.dabs.back().alpha)
            || !nearlyEqual(partlyDisabled.dabs.back().ellipseScaleX, 1.0)
            || !nearlyEqual(partlyDisabled.dabs[1].grain, 0.0)) {
        return 1;
    }

    BrushState rasterizerEnabled;
    rasterizerEnabled.rasterizer.brushSize = 10.0;
    rasterizerEnabled.rasterizer.spacingRatio = 0.25;
    rasterizerEnabled.rasterizer.flow = 0.25;
    const StrokeCommand flowEnabled = makeStrokeCommand(expressiveInput(), rasterizerEnabled, Stabilizer{0.0});
    BrushState flowDisabledBrush = rasterizerEnabled;
    flowDisabledBrush.rasterizer.flowEnabled = false;
    const StrokeCommand flowDisabled = makeStrokeCommand(expressiveInput(), flowDisabledBrush, Stabilizer{0.0});
    if (flowEnabled.dabs.empty()
            || flowDisabled.dabs.empty()
            || !(flowDisabled.dabs.front().alpha > flowEnabled.dabs.front().alpha)
            || !nearlyEqual(flowDisabled.dabs.front().alpha, 1.0)) {
        return 1;
    }

    BrushState spacingDisabledBrush = rasterizerEnabled;
    spacingDisabledBrush.rasterizer.spacingEnabled = false;
    const StrokeCommand denseSpacing = makeStrokeCommand(expressiveInput(), rasterizerEnabled, Stabilizer{0.0});
    const StrokeCommand defaultSpacing = makeStrokeCommand(expressiveInput(), spacingDisabledBrush, Stabilizer{0.0});
    if (denseSpacing.dabs.empty()
            || defaultSpacing.dabs.empty()
            || !(denseSpacing.dabs.size() > defaultSpacing.dabs.size())) {
        return 1;
    }

    Rasterizer opacityRasterizer;
    opacityRasterizer.argb = 0xFFFFFFFFU;
    opacityRasterizer.radius = 1;
    opacityRasterizer.opacity = 0.25;
    BrushDab dab;
    dab.position = {10.0, 10.0};
    dab.alpha = 1.0;
    dab.opacityCapScale = 1.0;
    dab.colorArgb = 0xFFFFFFFFU;
    const std::vector<RasterSample> opacityEnabledSamples = projectBrushDabs({dab}, opacityRasterizer);
    opacityRasterizer.opacityEnabled = false;
    const std::vector<RasterSample> opacityDisabledSamples = projectBrushDabs({dab}, opacityRasterizer);
    if (opacityCapAt(opacityEnabledSamples, {10, 10}) >= opacityCapAt(opacityDisabledSamples, {10, 10})
            || opacityCapAt(opacityDisabledSamples, {10, 10}) != 255) {
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
    hardnessRasterizer.hardness = 0.5;
    dab.scale = 2.0;
    const std::vector<RasterSample> hardnessEnabledSamples = projectBrushDabs({dab}, hardnessRasterizer);
    hardnessRasterizer.hardnessEnabled = false;
    const std::vector<RasterSample> hardnessDisabledSamples = projectBrushDabs({dab}, hardnessRasterizer);
    if (alphaAt(hardnessEnabledSamples, {10, 10}) >= alphaAt(hardnessDisabledSamples, {10, 10})
            || alphaAt(hardnessDisabledSamples, {10, 10}) != 64) {
        return 1;
    }

    return 0;
}
