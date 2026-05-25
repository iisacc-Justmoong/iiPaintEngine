#include <cmath>

#include "Stroke/StrokeCommand.h"
#include "Stroke/StrokeInput.h"
#include "Stroke/StrokeResampler.h"

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
            StrokePoint{{10.0, 0.0}, 0.4, 1.0, 0.0, 0.0, 0.0, 1},
            StrokePoint{{20.0, 10.0}, 0.8, 2.0, 0.0, 0.0, 1.0, 1},
            StrokePoint{{30.0, 0.0}, 0.6, 3.0, 0.0, 1.0, 0.0, 1},
    }};

    StrokeResampler resampler{};
    resampler.sampleSpacing = 2.0;
    resampler.mode = StrokeInterpolationMode::CatmullRom;

    const StrokeInput renderedInput = resampleStrokeInput(rawInput, resampler);
    if (renderedInput.points.size() <= rawInput.points.size()) {
        return 1;
    }

    if (!nearlyEqual(rawInput.points[1].position.y, 0.0)
            || !nearlyEqual(rawInput.points[1].time, 1.0)
            || rawInput.points[1].arcLength != 0.0) {
        return 1;
    }

    if (!nearlyEqual(renderedInput.points.front().time, rawInput.points.front().time)
            || !nearlyEqual(renderedInput.points.back().time, rawInput.points.back().time)) {
        return 1;
    }

    bool foundCurvedMidpoint = false;
    Types::Scalar previousTime = renderedInput.points.front().time;
    Types::Scalar previousArcLength = renderedInput.points.front().arcLength;
    for (const StrokePoint &sample : renderedInput.points) {
        if (sample.time < previousTime || sample.arcLength < previousArcLength) {
            return 1;
        }
        previousTime = sample.time;
        previousArcLength = sample.arcLength;

        if (std::abs(sample.position.x - 15.0) < 1.25
                && sample.position.y > 5.1
                && sample.time > 1.0
                && sample.time < 2.0
                && sample.arcLength > 10.0) {
            foundCurvedMidpoint = true;
        }
    }

    if (!foundCurvedMidpoint || renderedInput.points.back().arcLength <= 30.0) {
        return 1;
    }

    BrushState brush{};
    brush.rasterizer.spacing = 3.0;
    brush.rasterizer.flow = 0.5;
    brush.resampler = resampler;

    const StrokeCommand command = makeStrokeCommand(rawInput, brush, Stabilizer{0.0});
    if (command.path.rawInput.points.size() != rawInput.points.size()
            || command.path.renderedInput.points.size() <= rawInput.points.size()
            || command.path.renderedCurve.samples.size() != command.path.renderedInput.points.size()) {
        return 1;
    }

    if (!nearlyEqual(command.path.rawInput.points[2].position.y, 10.0)
            || command.path.rawInput.points[2].arcLength != 0.0
            || command.path.renderedCurve.samples.back().arcLength <= 30.0) {
        return 1;
    }

    return 0;
}
