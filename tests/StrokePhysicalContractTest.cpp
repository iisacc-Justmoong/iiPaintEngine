#include <cmath>
#include <vector>

#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCommand.h"
#include "Stroke/StrokeCurve.h"
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
            StrokePoint{{0.0, 0.0}, 0.25, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{5.0, 0.0}, 0.75, 1.0, 0.0, 0.0, 0.5, 1},
            StrokePoint{{5.0, 5.0}, 0.5, 2.0, 0.0, 1.0, 0.0, 1},
    }};

    BrushState brush{};
    brush.randomSeed = 17;
    brush.rasterizer.brushSize = 6.0;
    brush.rasterizer.spacingRatio = 0.5;
    brush.rasterizer.warmupDistance = 3.0;
    brush.rasterizer.taperDistance = 3.0;
    brush.rasterizer.flow = 0.4;
    brush.rasterizer.rotationJitter = 0.25;

    const StrokeCommand command = makeStrokeCommand(rawInput, brush, Stabilizer{1.0});
    if (command.path.rawInput.points.size() != 3 || command.path.renderedInput.points.size() <= 3) {
        return 1;
    }

    if (!nearlyEqual(command.path.rawInput.points[1].position.x, 5.0)
            || !nearlyEqual(command.path.rawInput.points[1].time, 1.0)
            || command.path.rawInput.points[1].deviceState != 1) {
        return 1;
    }

    bool foundSmoothedMiddleTimestamp = false;
    for (const StrokePoint &renderedSample : command.path.renderedInput.points) {
        if (nearlyEqual(renderedSample.time, command.path.rawInput.points[1].time)
                && !nearlyEqual(renderedSample.position.x, command.path.rawInput.points[1].position.x)) {
            foundSmoothedMiddleTimestamp = true;
        }
    }

    if (!foundSmoothedMiddleTimestamp) {
        return 1;
    }

    const StrokeCurve rawCurve = makeStrokeCurve(rawInput);
    Rasterizer rasterizer = brush.rasterizer;
    rasterizer.warmupDistance = 0.0;
    rasterizer.taperDistance = 0.0;
    rasterizer.rotationJitter = 0.0;

    const std::vector<BrushDab> dabs = placeBrushDabs(rawCurve, rasterizer, brush.randomSeed);
    if (dabs.size() != 5) {
        return 1;
    }

    if (!nearlyEqual(dabs[0].position.x, 0.0)
            || !nearlyEqual(dabs[1].position.x, 3.0)
            || !nearlyEqual(dabs[2].position.x, 5.0)
            || !nearlyEqual(dabs[2].position.y, 1.0)
            || !nearlyEqual(dabs[3].position.y, 4.0)
            || !nearlyEqual(dabs[4].position.y, 5.0)) {
        return 1;
    }

    if (nearlyEqual(dabs[2].position.y, 0.0)) {
        return 1;
    }

    const std::vector<BrushDab> seededAgain = placeBrushDabs(rawCurve, brush.rasterizer, brush.randomSeed);
    const std::vector<BrushDab> differentSeed = placeBrushDabs(rawCurve, brush.rasterizer, brush.randomSeed + 1);
    if (seededAgain.size() != differentSeed.size()
            || !nearlyEqual(seededAgain[1].rotationRadians, placeBrushDabs(rawCurve, brush.rasterizer, brush.randomSeed)[1].rotationRadians)
            || nearlyEqual(seededAgain[1].rotationRadians, differentSeed[1].rotationRadians)) {
        return 1;
    }

    if (!(seededAgain.front().alpha < seededAgain[1].alpha)
            || !(seededAgain.back().alpha < seededAgain[1].alpha)) {
        return 1;
    }

    if (command.dabs.empty()
            || command.dirtyBounds.width <= 0
            || command.dirtyBounds.height <= 0
            || command.dirtyBounds.origin.x > 0
            || command.dirtyBounds.origin.y > 0) {
        return 1;
    }

    return 0;
}
