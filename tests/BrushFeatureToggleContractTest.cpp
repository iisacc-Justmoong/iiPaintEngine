#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "tests/RasterDabTestUtils.h"

namespace {

template <typename T, typename = void>
struct HasPressureToHardness : std::false_type {
};

template <typename T>
struct HasPressureToHardness<T, std::void_t<decltype(&T::pressureToHardness)>> : std::true_type {
};

template <typename T, typename = void>
struct HasPressureToHardnessEnabled : std::false_type {
};

template <typename T>
struct HasPressureToHardnessEnabled<T, std::void_t<decltype(&T::pressureToHardnessEnabled)>>
    : std::true_type {
};

static_assert(!HasPressureToHardness<BrushDynamics>::value,
              "BrushDynamics must not expose pressureToHardness");
static_assert(!HasPressureToHardnessEnabled<BrushDynamics>::value,
              "BrushDynamics must not expose pressureToHardnessEnabled");

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

std::vector<BrushDab> expressiveDabs(const BrushState &brush, bool neutralPressure = false)
{
    const Types::Scalar firstPressure = neutralPressure ? 1.0 : 0.2;
    const Types::Scalar lastPressure = neutralPressure ? 1.0 : 0.8;
    return streamTestDabs(brush,
                          StrokePoint{{0.0, 0.0}, firstPressure, 0.0, 0.0, 0.0, 1.0, 1},
                          StrokePoint{{16.0, 0.0}, lastPressure, 0.2, 0.0, 0.0, 1.0, 1});
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
    const std::vector<BrushDab> withoutPressure = expressiveDabs(pressureDisabled);
    const std::vector<BrushDab> neutralPressure = expressiveDabs(pressureDisabled, true);
    if (withoutPressure.empty()
            || neutralPressure.empty()
            || !nearlyEqual(withoutPressure.front().scale, neutralPressure.front().scale)
            || !nearlyEqual(withoutPressure.back().alpha, neutralPressure.back().alpha)
            || !nearlyEqual(withoutPressure.back().opacityCapScale, neutralPressure.back().opacityCapScale)
            || !nearlyEqual(withoutPressure.back().hardnessScale, neutralPressure.back().hardnessScale)) {
        return 1;
    }

    BrushState velocityEnabled = sensitiveBrush();
    BrushState velocityDisabled = velocityEnabled;
    velocityDisabled.dynamics.velocityInputEnabled = false;
    const std::vector<BrushDab> fastVelocity = expressiveDabs(velocityEnabled);
    const std::vector<BrushDab> ignoredVelocity = expressiveDabs(velocityDisabled);
    if (fastVelocity.empty()
            || ignoredVelocity.empty()
            || ignoredVelocity.size() <= fastVelocity.size()
            || !(ignoredVelocity.back().alpha > fastVelocity.back().alpha)) {
        return 1;
    }

    BrushState tiltDisabled = sensitiveBrush();
    tiltDisabled.dynamics.tiltInputEnabled = false;
    tiltDisabled.dynamics.randomInputEnabled = false;
    const std::vector<BrushDab> withoutTilt = expressiveDabs(tiltDisabled);
    if (withoutTilt.empty()
            || !nearlyEqual(withoutTilt.back().rotationRadians, 0.0)
            || !nearlyEqual(withoutTilt.back().ellipseScaleX, 1.0)
            || !nearlyEqual(withoutTilt.back().ellipseScaleY, 1.0)
            || !nearlyEqual(withoutTilt.back().textureDirectionRadians, 0.0)) {
        return 1;
    }

    BrushState randomDisabled = sensitiveBrush();
    randomDisabled.dynamics.randomInputEnabled = false;
    const std::vector<BrushDab> firstSeed = expressiveDabs(randomDisabled);
    randomDisabled.randomSeed += 1;
    const std::vector<BrushDab> secondSeed = expressiveDabs(randomDisabled);
    if (firstSeed.size() < 2
            || secondSeed.size() < 2
            || !nearlyEqual(firstSeed[1].rotationRadians, secondSeed[1].rotationRadians)
            || !nearlyEqual(firstSeed[1].grain, secondSeed[1].grain)) {
        return 1;
    }

    BrushState perMappingEnabled = sensitiveBrush();
    BrushState perMappingDisabled = perMappingEnabled;
    perMappingDisabled.dynamics.pressureToSizeEnabled = false;
    perMappingDisabled.dynamics.velocityToDryOutEnabled = false;
    perMappingDisabled.dynamics.tiltToEllipseEnabled = false;
    perMappingDisabled.dynamics.rotationJitterEnabled = false;
    perMappingDisabled.dynamics.grainJitterEnabled = false;
    const std::vector<BrushDab> mappingEnabled = expressiveDabs(perMappingEnabled);
    const std::vector<BrushDab> partlyDisabled = expressiveDabs(perMappingDisabled);
    const Types::Scalar expectedDirectPressureScale = 1.0
            + (0.8 - 1.0) * perMappingDisabled.rasterizer.pressureScale;
    if (partlyDisabled.size() < 2
            || mappingEnabled.size() < 2
            || !nearlyEqual(partlyDisabled.back().scale, expectedDirectPressureScale)
            || !(partlyDisabled.back().scale > mappingEnabled.back().scale)
            || !(partlyDisabled.back().alpha > mappingEnabled.back().alpha)
            || !nearlyEqual(partlyDisabled.back().hardnessScale, 1.0)
            || !nearlyEqual(mappingEnabled.back().hardnessScale, 1.0)
            || !nearlyEqual(partlyDisabled.back().ellipseScaleX, 1.0)
            || !nearlyEqual(partlyDisabled[1].grain, 0.0)) {
        return 1;
    }

    BrushState opacityMappingEnabled = sensitiveBrush();
    opacityMappingEnabled.dynamics.pressureToFlowEnabled = false;
    opacityMappingEnabled.dynamics.velocityToOpacityEnabled = false;
    BrushState opacityMappingDisabled = opacityMappingEnabled;
    opacityMappingDisabled.dynamics.pressureToOpacityEnabled = false;
    const std::vector<BrushDab> pressureOpacityEnabled = expressiveDabs(opacityMappingEnabled);
    const std::vector<BrushDab> pressureOpacityDisabled = expressiveDabs(opacityMappingDisabled);
    if (pressureOpacityEnabled.size() < 2
            || pressureOpacityDisabled.size() < 2
            || !(pressureOpacityDisabled.front().opacityCapScale > pressureOpacityEnabled.front().opacityCapScale)
            || !nearlyEqual(pressureOpacityDisabled.front().opacityCapScale, 1.0)
            || !nearlyEqual(pressureOpacityDisabled.back().opacityCapScale, 1.0)
            || !nearlyEqual(pressureOpacityDisabled.front().alpha, pressureOpacityEnabled.front().alpha)) {
        return 1;
    }

    BrushState rasterizerEnabled;
    rasterizerEnabled.rasterizer.brushSize = 10.0;
    rasterizerEnabled.rasterizer.spacingRatio = 0.25;
    rasterizerEnabled.rasterizer.flow = 0.25;
    const std::vector<BrushDab> flowEnabled = expressiveDabs(rasterizerEnabled);
    BrushState flowDisabledBrush = rasterizerEnabled;
    flowDisabledBrush.rasterizer.flowEnabled = false;
    const std::vector<BrushDab> flowDisabled = expressiveDabs(flowDisabledBrush);
    if (flowEnabled.empty()
            || flowDisabled.empty()
            || !(flowDisabled.front().alpha > flowEnabled.front().alpha)
            || !nearlyEqual(flowDisabled.front().alpha, 1.0)) {
        return 1;
    }

    BrushState spacingDisabledBrush = rasterizerEnabled;
    spacingDisabledBrush.rasterizer.spacingEnabled = false;
    const std::vector<BrushDab> denseSpacing = expressiveDabs(rasterizerEnabled);
    const std::vector<BrushDab> defaultSpacing = expressiveDabs(spacingDisabledBrush);
    if (denseSpacing.empty()
            || defaultSpacing.empty()
            || !(defaultSpacing.size() > denseSpacing.size())) {
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
