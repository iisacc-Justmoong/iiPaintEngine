#include <cmath>

#include "Brush/BrushPresetSerializer.h"
#include "tests/RasterDabTestUtils.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

void configureCurve(BrushDynamicsResponseCurve &curve,
                    Types::Scalar min,
                    Types::Scalar center,
                    Types::Scalar max,
                    BrushDynamicsEasing easing = BrushDynamicsEasing::Linear,
                    Types::Scalar jitter = 0.0)
{
    curve.enabled = true;
    curve.min = min;
    curve.center = center;
    curve.max = max;
    curve.easing = easing;
    curve.jitter = jitter;
}

std::vector<BrushDab> expressiveDabs(const BrushState &brush)
{
    return streamTestDabs(brush,
                          StrokePoint{{0.0, 0.0}, 0.0, 0.0, 1.0, 0.0, 0.0, 1, 0.0},
                          StrokePoint{{10.0, 0.0}, 1.0, 1.0, 1.0, 0.0, 1.0, 1, 10.0});
}

} // namespace

int main()
{
    BrushDynamics graphDynamics;
    graphDynamics.sizeResponse.enabled = true;
    configureCurve(graphDynamics.sizeResponse.pressure, 0.25, 0.6, 1.2);
    if (!nearlyEqual(resolveBrushDynamics(graphDynamics, BrushDynamicsInput{0.0}).sizeScale, 0.25)
            || !nearlyEqual(resolveBrushDynamics(graphDynamics, BrushDynamicsInput{0.5}).sizeScale, 0.6)
            || !nearlyEqual(resolveBrushDynamics(graphDynamics, BrushDynamicsInput{1.0}).sizeScale, 1.2)) {
        return 1;
    }

    BrushDynamics easedDynamics;
    easedDynamics.sizeResponse.enabled = true;
    configureCurve(easedDynamics.sizeResponse.pressure, 0.0, 0.5, 1.0, BrushDynamicsEasing::EaseOut);
    const Types::Scalar easedQuarter = resolveBrushDynamics(easedDynamics, BrushDynamicsInput{0.25}).sizeScale;
    if (!(easedQuarter > 0.25 && easedQuarter < 0.5)) {
        return 2;
    }

    BrushDynamics crossInputDynamics;
    crossInputDynamics.flowResponse.enabled = true;
    configureCurve(crossInputDynamics.flowResponse.pressure, 0.5, 0.75, 1.0);
    configureCurve(crossInputDynamics.flowResponse.velocity, 1.0, 0.8, 0.5);
    const BrushDynamicsResult crossInput = resolveBrushDynamics(
            crossInputDynamics,
            BrushDynamicsInput{1.0, 1.0});
    BrushDynamics withoutVelocity = crossInputDynamics;
    withoutVelocity.velocityInputEnabled = false;
    const BrushDynamicsResult velocityIgnored = resolveBrushDynamics(
            withoutVelocity,
            BrushDynamicsInput{1.0, 1.0});
    if (!nearlyEqual(crossInput.flowScale, 0.5)
            || !nearlyEqual(velocityIgnored.flowScale, 1.0)) {
        return 3;
    }

    BrushDynamics jitterDynamics;
    jitterDynamics.opacityResponse.enabled = true;
    configureCurve(jitterDynamics.opacityResponse.pressure, 0.5, 1.0, 1.5, BrushDynamicsEasing::Linear, 0.25);
    const BrushDynamicsResult jittered = resolveBrushDynamics(
            jitterDynamics,
            BrushDynamicsInput{0.5, 0.0, 0.0, 0.0, 1.0, 0.0});
    jitterDynamics.randomInputEnabled = false;
    const BrushDynamicsResult noJitter = resolveBrushDynamics(
            jitterDynamics,
            BrushDynamicsInput{0.5, 0.0, 0.0, 0.0, 1.0, 0.0});
    if (!nearlyEqual(jittered.opacityScale, 1.25)
            || !nearlyEqual(noJitter.opacityScale, 1.0)) {
        return 4;
    }

    BrushDynamics expressiveDynamics;
    expressiveDynamics.scatterResponse.enabled = true;
    configureCurve(expressiveDynamics.scatterResponse.pressure, 0.0, 0.5, 1.0);
    expressiveDynamics.rotationResponse.enabled = true;
    configureCurve(expressiveDynamics.rotationResponse.pressure, -0.25, 0.0, 0.25);
    expressiveDynamics.textureDepthResponse.enabled = true;
    configureCurve(expressiveDynamics.textureDepthResponse.pressure, 0.2, 0.6, 1.4);
    expressiveDynamics.wetnessResponse.enabled = true;
    configureCurve(expressiveDynamics.wetnessResponse.pressure, 0.2, 0.6, 1.0);
    expressiveDynamics.dryOutResponse.enabled = true;
    configureCurve(expressiveDynamics.dryOutResponse.velocity, 1.0, 0.75, 0.5);
    expressiveDynamics.bristleSpreadResponse.enabled = true;
    configureCurve(expressiveDynamics.bristleSpreadResponse.tilt, 1.0, 1.5, 2.0);
    const BrushDynamicsResult expressiveResult = resolveBrushDynamics(
            expressiveDynamics,
            BrushDynamicsInput{1.0, 1.0, 0.0, 1.0});
    if (!nearlyEqual(expressiveResult.scatterScale, 1.0)
            || !nearlyEqual(expressiveResult.rotationOffsetRadians, 0.25)
            || !nearlyEqual(expressiveResult.textureDepthScale, 1.4)
            || !nearlyEqual(expressiveResult.wetnessScale, 1.0)
            || !nearlyEqual(expressiveResult.dryOutScale, 0.5)
            || !nearlyEqual(expressiveResult.bristleSpreadScale, 2.0)) {
        return 5;
    }

    BrushState brush;
    brush.randomSeed = 99;
    brush.rasterizer.brushSize = 10.0;
    brush.rasterizer.spacingRatio = 1.0;
    brush.rasterizer.flow = 1.0;
    brush.dynamics = expressiveDynamics;
    brush.dynamics.sizeResponse.enabled = true;
    configureCurve(brush.dynamics.sizeResponse.pressure, 0.25, 0.6, 1.0);
    brush.material.scatter.enabled = true;
    brush.material.scatter.radius = 4.0;
    brush.material.texture.enabled = true;
    brush.material.texture.width = 1;
    brush.material.texture.height = 1;
    brush.material.texture.alpha = {128};
    brush.material.texture.grainStrength = 0.5;
    brush.material.simulation.enabled = true;
    brush.material.simulation.model = BrushSimulationModel::WetPaint;
    brush.material.simulation.wetness = 1.0;
    brush.material.bristle.enabled = true;
    brush.material.bristle.shape = BristleShape::Flat;
    brush.material.bristle.count = 16;
    brush.material.bristle.length = 4.0;
    brush.material.bristle.stiffness = 0.5;
    const std::vector<BrushDab> dabs = expressiveDabs(brush);
    if (dabs.size() < 2) {
        return 6;
    }
    const BrushDab &first = dabs.front();
    const BrushDab &last = dabs.back();
    if (!nearlyEqual(first.scatterScale, 0.0)
            || !nearlyEqual(first.position.x, 0.0)
            || !(last.scale > first.scale)
            || !(last.rotationRadians > first.rotationRadians)
            || !(last.textureDepthScale > first.textureDepthScale)
            || !(last.wetnessScale > first.wetnessScale)
            || !nearlyEqual(last.dryOutScale, 0.5)
            || !(last.bristleSpreadScale > first.bristleSpreadScale)
            || !(last.ellipseScaleX > first.ellipseScaleX)) {
        return 7;
    }

    BrushPreset preset;
    preset.dynamics = brush.dynamics;
    const BrushPreset reopened = deserializeBrushPreset(serializeBrushPreset(preset));
    if (!reopened.dynamics.sizeResponse.enabled
            || !reopened.dynamics.bristleSpreadResponse.tilt.enabled
            || reopened.dynamics.sizeResponse.pressure.min != 0.25
            || reopened.dynamics.rotationResponse.pressure.max != 0.25
            || reopened.dynamics.dryOutResponse.velocity.max != 0.5
            || reopened.dynamics.sizeResponse.pressure.easing != BrushDynamicsEasing::Linear) {
        return 8;
    }

    return 0;
}
