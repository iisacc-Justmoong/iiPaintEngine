#include <cmath>
#include <vector>

#include "Stroke/StrokeCommand.h"
#include "Stroke/StrokeInput.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

} // namespace

int main()
{
    StrokeInput rawInput{{
            StrokePoint{{0.0, 0.0}, 0.2, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{10.0, 0.0}, 0.8, 1.0, 0.0, 0.0, 1.0, 1},
    }};

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

    const StrokeCommand command = makeStrokeCommand(rawInput, brush, Stabilizer{0.0});
    if (command.dabs.size() < 3) {
        return 1;
    }

    const BrushDab &first = command.dabs.front();
    const BrushDab &last = command.dabs.back();

    if (!(last.scale > first.scale)) {
        return 1;
    }

    if (!(last.alpha < brush.rasterizer.flow)
            || !(last.opacityCapScale < 1.0)) {
        return 1;
    }

    if (!(last.ellipseScaleX > 1.0)
            || !(last.ellipseScaleY < 1.0)
            || !nearlyEqual(last.textureDirectionRadians, std::atan2(1.0, 0.0))) {
        return 1;
    }

    const StrokeCommand sameSeed = makeStrokeCommand(rawInput, brush, Stabilizer{0.0});
    brush.randomSeed += 1;
    const StrokeCommand differentSeed = makeStrokeCommand(rawInput, brush, Stabilizer{0.0});

    if (!nearlyEqual(command.dabs[1].rotationRadians, sameSeed.dabs[1].rotationRadians)
            || !nearlyEqual(command.dabs[1].grain, sameSeed.dabs[1].grain)
            || nearlyEqual(command.dabs[1].rotationRadians, differentSeed.dabs[1].rotationRadians)
            || nearlyEqual(command.dabs[1].grain, differentSeed.dabs[1].grain)) {
        return 1;
    }

    return 0;
}
