#include <cmath>
#include <cstddef>
#include <iostream>

#include "Stroke/LiveStroke.h"
#include "Stroke/StrokeCommand.h"
#include "Stroke/StrokeGeometry.h"
#include "Stroke/StrokeInput.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

int fail(const char *message)
{
    std::cerr << message << '\n';
    return 1;
}

StrokeInput makeAngularStroke()
{
    return StrokeInput{{
            StrokePoint{{0.0, 0.0}, 0.2, 0.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{6.0, 0.0}, 0.5, 1.0, 0.0, 0.3, 0.4, 1},
            StrokePoint{{6.0, 8.0}, 1.0, 3.0, 0.0, 0.0, 0.7, 1},
            StrokePoint{{2.0, 10.0}, 0.4, 4.0, 0.0, -0.6, 0.2, 1},
    }};
}

} // namespace

int main()
{
    const StrokeInput rawInput = makeAngularStroke();
    const StrokeGeometryReport geometry = describeStrokeGeometry(rawInput);

    if (geometry.empty
            || geometry.sourcePointCount != 4
            || geometry.sampleCount != 4
            || geometry.segmentCount != 3
            || geometry.samples.size() != 4
            || geometry.segments.size() != 3) {
        return fail("shape counts");
    }

    const Types::Scalar expectedPathLength = 14.0 + std::sqrt(20.0);
    if (!nearlyEqual(geometry.bounds.origin.x, 0.0)
            || !nearlyEqual(geometry.bounds.origin.y, 0.0)
            || !nearlyEqual(geometry.bounds.width, 6.0)
            || !nearlyEqual(geometry.bounds.height, 10.0)
            || !nearlyEqual(geometry.pathLength, expectedPathLength)
            || !nearlyEqual(geometry.chordLength, std::sqrt(104.0))
            || !nearlyEqual(geometry.duration, 4.0)
            || !nearlyEqual(geometry.signedArea, 46.0)
            || !(geometry.straightness > 0.55 && geometry.straightness < 0.56)) {
        return fail("bounds and scalar geometry");
    }

    if (!nearlyEqual(geometry.centroid.x, 3.5)
            || !nearlyEqual(geometry.centroid.y, 4.5)
            || !(geometry.lengthWeightedCentroid.x > 4.53 && geometry.lengthWeightedCentroid.x < 4.55)
            || !(geometry.lengthWeightedCentroid.y > 3.90 && geometry.lengthWeightedCentroid.y < 3.93)) {
        return fail("centroids");
    }

    if (!nearlyEqual(geometry.pressure.minimum, 0.2)
            || !nearlyEqual(geometry.pressure.maximum, 1.0)
            || !nearlyEqual(geometry.pressure.average, 0.525)
            || !nearlyEqual(geometry.velocity.minimum, 4.0)
            || !nearlyEqual(geometry.velocity.maximum, 6.0)
            || !nearlyEqual(geometry.tiltMagnitude.maximum, 0.7)) {
        return fail("statistics");
    }

    if (!nearlyEqual(geometry.segments[0].length, 6.0)
            || !nearlyEqual(geometry.segments[0].headingRadians, 0.0)
            || !nearlyEqual(geometry.segments[1].length, 8.0)
            || !nearlyEqual(geometry.segments[1].headingRadians, std::acos(-1.0) / 2.0)
            || !nearlyEqual(geometry.segments[1].velocity, 4.0)
            || !nearlyEqual(geometry.segments[1].pressureDelta, 0.5)
            || !nearlyEqual(geometry.segments[1].pressurePerDistance, 0.0625)) {
        return fail("segments");
    }

    if (!nearlyEqual(geometry.samples[0].normalizedArcLength, 0.0)
            || !nearlyEqual(geometry.samples[3].normalizedArcLength, 1.0)
            || !nearlyEqual(geometry.samples[1].signedTurnRadians, std::acos(-1.0) / 2.0)
            || !(geometry.samples[1].signedCurvature > 0.22 && geometry.samples[1].signedCurvature < 0.23)
            || !(geometry.samples[2].signedTurnRadians > 1.10 && geometry.samples[2].signedTurnRadians < 1.11)
            || !(geometry.totalAbsoluteTurnRadians > 2.67 && geometry.totalAbsoluteTurnRadians < 2.68)
            || geometry.inflectionCount != 0
            || geometry.cuspCount != 1) {
        return fail("sample curvature");
    }

    if (!(geometry.principalAxisRadians > 1.48 && geometry.principalAxisRadians < 1.49)
            || !(geometry.majorSpread > geometry.minorSpread)
            || !(geometry.elongation > 1.0)) {
        return fail("principal axes");
    }

    BrushState brush{};
    brush.rasterizer.brushSize = 4.0;
    brush.rasterizer.spacingRatio = 0.5;
    brush.rasterizer.rotationJitter = 0.0;
    brush.resampler.sampleSpacing = 2.0;

    const StrokeCommand command = makeStrokeCommand(rawInput, brush, Stabilizer{0.0});
    if (command.path.rawGeometry.sampleCount != rawInput.points.size()
            || command.path.renderedGeometry.sampleCount <= command.path.rawGeometry.sampleCount
            || command.path.renderedGeometry.pathLength < command.path.rawGeometry.pathLength
            || !nearlyEqual(command.path.renderedGeometry.samples.back().normalizedArcLength, 1.0)) {
        return fail("command geometry");
    }

    const LiveStrokeFrame liveFrame = makeLiveStrokeFrame(rawInput, brush, Stabilizer{0.0}, false);
    if (!liveFrame.active
            || liveFrame.rawGeometry.sampleCount != rawInput.points.size()
            || liveFrame.displayedGeometry.sampleCount <= liveFrame.rawGeometry.sampleCount
            || liveFrame.displayedGeometry.segments.empty()
            || !liveFrame.samples.empty()) {
        return fail("live geometry");
    }

    return 0;
}
