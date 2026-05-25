//
// Created by Justmoong on 2026 May 24.
//

#include "StrokeResampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

Types::Scalar clamp01(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Scalar distance(CanvasPoint lhs, CanvasPoint rhs)
{
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

CanvasPoint linearPoint(CanvasPoint start, CanvasPoint end, Types::Scalar t)
{
    return CanvasPoint{
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
    };
}

Types::Scalar linearScalar(Types::Scalar start, Types::Scalar end, Types::Scalar t)
{
    return start + (end - start) * t;
}

CanvasPoint catmullRomPoint(CanvasPoint p0,
                            CanvasPoint p1,
                            CanvasPoint p2,
                            CanvasPoint p3,
                            Types::Scalar t)
{
    const Types::Scalar t2 = t * t;
    const Types::Scalar t3 = t2 * t;
    return CanvasPoint{
            0.5 * ((2.0 * p1.x)
                   + (-p0.x + p2.x) * t
                   + (2.0 * p0.x - 5.0 * p1.x + 4.0 * p2.x - p3.x) * t2
                   + (-p0.x + 3.0 * p1.x - 3.0 * p2.x + p3.x) * t3),
            0.5 * ((2.0 * p1.y)
                   + (-p0.y + p2.y) * t
                   + (2.0 * p0.y - 5.0 * p1.y + 4.0 * p2.y - p3.y) * t2
                   + (-p0.y + 3.0 * p1.y - 3.0 * p2.y + p3.y) * t3),
    };
}

const StrokePoint &controlPoint(const StrokeInput &input, std::size_t index)
{
    return input.points[std::min(index, input.points.size() - 1)];
}

StrokePoint interpolateStrokePoint(const StrokeInput &input,
                                   std::size_t segmentIndex,
                                   Types::Scalar t,
                                   StrokeInterpolationMode mode)
{
    const StrokePoint &start = input.points[segmentIndex];
    const StrokePoint &end = input.points[segmentIndex + 1];
    const StrokePoint &p0 = segmentIndex > 0 ? input.points[segmentIndex - 1] : start;
    const StrokePoint &p3 = controlPoint(input, segmentIndex + 2);

    StrokePoint sample;
    sample.position = mode == StrokeInterpolationMode::CatmullRom
            ? catmullRomPoint(p0.position, start.position, end.position, p3.position, t)
            : linearPoint(start.position, end.position, t);
    sample.pressure = linearScalar(start.pressure, end.pressure, t);
    sample.time = linearScalar(start.time, end.time, t);
    sample.velocity = linearScalar(start.velocity, end.velocity, t);
    sample.tiltX = linearScalar(start.tiltX, end.tiltX, t);
    sample.tiltY = linearScalar(start.tiltY, end.tiltY, t);
    sample.deviceState = t < 1.0 ? start.deviceState : end.deviceState;
    return sample;
}

void appendWithArcLength(StrokeInput &output, StrokePoint sample)
{
    if (output.points.empty()) {
        sample.arcLength = 0.0;
        output.points.push_back(sample);
        return;
    }

    StrokePoint &previous = output.points.back();
    if (previous.position.x == sample.position.x
            && previous.position.y == sample.position.y
            && previous.time == sample.time) {
        return;
    }

    const Types::Scalar segmentLength = distance(previous.position, sample.position);
    const Types::Scalar dt = sample.time - previous.time;
    sample.arcLength = previous.arcLength + segmentLength;
    sample.velocity = dt > 0.0 ? segmentLength / dt : 0.0;
    output.points.push_back(sample);
}

} // namespace

StrokeInput resampleStrokeInput(const StrokeInput &input, const StrokeResampler &resampler)
{
    StrokeInput output;
    if (input.points.size() < 2) {
        output = input;
        if (!output.points.empty()) {
            output.points.front().arcLength = 0.0;
        }
        return output;
    }

    const Types::Scalar spacing = std::max<Types::Scalar>(0.25, resampler.sampleSpacing);
    appendWithArcLength(output, input.points.front());

    for (std::size_t segmentIndex = 0; segmentIndex + 1 < input.points.size(); ++segmentIndex) {
        const StrokePoint &start = input.points[segmentIndex];
        const StrokePoint &end = input.points[segmentIndex + 1];
        const Types::Scalar segmentLength = std::max<Types::Scalar>(0.0, distance(start.position, end.position));
        const int steps = std::max(1, static_cast<int>(std::ceil(segmentLength / spacing)));

        for (int step = 1; step <= steps; ++step) {
            const Types::Scalar t = clamp01(static_cast<Types::Scalar>(step) / static_cast<Types::Scalar>(steps));
            appendWithArcLength(output, interpolateStrokePoint(input, segmentIndex, t, resampler.mode));
        }
    }

    return output;
}

StrokeCurve makeResampledStrokeCurve(const StrokeInput &input, const StrokeResampler &resampler)
{
    return makeStrokeCurve(resampleStrokeInput(input, resampler));
}
