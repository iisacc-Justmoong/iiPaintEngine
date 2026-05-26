#include <cmath>

#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCommand.h"
#include "Stroke/StrokeCurve.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

StrokeInput cuspStroke()
{
    return StrokeInput{{
            StrokePoint{{0.0, 0.0}, 1.0, 0.0},
            StrokePoint{{5.0, 10.0}, 1.0, 1.0},
            StrokePoint{{10.0, 0.0}, 1.0, 2.0},
            StrokePoint{{15.0, 0.0}, 1.0, 3.0},
    }};
}

StrokeInput straightStroke()
{
    return StrokeInput{{
            StrokePoint{{0.0, 0.0}, 1.0, 0.0},
            StrokePoint{{10.0, 0.0}, 1.0, 1.0},
            StrokePoint{{20.0, 0.0}, 1.0, 2.0},
    }};
}

StrokeInput speedChangingStroke()
{
    return StrokeInput{{
            StrokePoint{{0.0, 0.0}, 1.0, 0.0},
            StrokePoint{{10.0, 0.0}, 1.0, 10.0},
            StrokePoint{{20.0, 0.0}, 1.0, 11.0},
    }};
}

} // namespace

int main()
{
    Stabilizer lineSmoothing;
    lineSmoothing.smoothing = 1.0;
    lineSmoothing.mode = StabilizerMode::Line;
    StrokeInput pulledLine = stabilizeStrokeInput(cuspStroke(), lineSmoothing);
    if (pulledLine.points[1].position.y >= 1.0) {
        return 1;
    }

    lineSmoothing.cuspPreservationEnabled = true;
    lineSmoothing.cuspAngleRadians = 1.0;
    StrokeInput preservedCusp = stabilizeStrokeInput(cuspStroke(), lineSmoothing);
    if (!nearlyEqual(preservedCusp.points[1].position.x, 5.0)
            || !nearlyEqual(preservedCusp.points[1].position.y, 10.0)) {
        return 2;
    }

    Stabilizer predictive;
    predictive.predictionEnabled = true;
    predictive.predictionHorizon = 0.5;
    predictive.latencyCompensation = 0.5;
    predictive.previewDabsMatchCursor = true;
    StrokeInput predictedInput = stabilizeStrokeInput(straightStroke(), predictive);
    if (predictedInput.points.back().position.x <= 20.0
            || predictedInput.points.back().time <= 2.0) {
        return 3;
    }

    BrushState brush;
    brush.rasterizer.radius = 0;
    brush.rasterizer.spacing = 5.0;
    brush.rasterizer.flow = 1.0;
    brush.rasterizer.opacity = 1.0;
    brush.resampler.mode = StrokeInterpolationMode::Linear;
    brush.resampler.sampleSpacing = 5.0;
    const LiveStrokeFrame frame = makeLiveStrokeFrame(straightStroke(), brush, predictive);
    if (!frame.cursorPreviewPositionValid
            || frame.dabs.empty()
            || !nearlyEqual(frame.cursorPreviewPosition.x, frame.dabs.back().position.x)
            || !nearlyEqual(frame.cursorPreviewPosition.y, frame.dabs.back().position.y)
            || frame.cursorPreviewPosition.x <= 20.0) {
        return 4;
    }

    const StrokeCommand command = makeStrokeCommand(straightStroke(), brush, predictive);
    if (command.dabs.empty()
            || !nearlyEqual(command.dabs.back().position.x, frame.dabs.back().position.x)
            || !nearlyEqual(command.dabs.back().position.y, frame.dabs.back().position.y)) {
        return 5;
    }

    Stabilizer adaptive;
    adaptive.adaptiveResamplingEnabled = true;
    adaptive.adaptiveMinSpacing = 1.0;
    adaptive.adaptiveMaxSpacing = 5.0;
    adaptive.adaptiveVelocityScale = 0.2;
    const StrokeInput adaptiveInput = stabilizeStrokeInput(speedChangingStroke(), adaptive);
    int slowInteriorSamples = 0;
    int fastInteriorSamples = 0;
    for (const StrokePoint &point : adaptiveInput.points) {
        if (point.time > 0.0 && point.time < 10.0) {
            ++slowInteriorSamples;
        }
        if (point.time > 10.0 && point.time < 11.0) {
            ++fastInteriorSamples;
        }
    }
    if (fastInteriorSamples <= slowInteriorSamples) {
        return 6;
    }

    Rasterizer taper;
    taper.spacing = 5.0;
    taper.flow = 1.0;
    taper.opacity = 1.0;
    taper.taperDistance = 20.0;
    taper.taperMinimum = 0.0;
    taper.endTaperShape = StrokeTaperShape::EaseIn;
    const std::vector<BrushDab> taperedDabs = placeBrushDabs(makeStrokeCurve(straightStroke()), taper);
    if (taperedDabs.size() < 3
            || taperedDabs[2].strokeDistance < 9.9
            || taperedDabs[2].strokeDistance > 10.1
            || taperedDabs[2].alpha < 0.24
            || taperedDabs[2].alpha > 0.26
            || taperedDabs.back().alpha != 0.0) {
        return 7;
    }

    return 0;
}
