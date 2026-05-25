//
// Created by Justmoong on 2026 May 24.
//

#include "StrokeCurve.h"

#include <cstddef>
#include <cmath>

StrokeCurve makeStrokeCurve(const StrokeInput &input)
{
    StrokeCurve curve;
    curve.samples.reserve(input.points.size());

    Types::Scalar arcLength = 0.0;
    for (std::size_t index = 0; index < input.points.size(); ++index) {
        StrokePoint sample = input.points[index];
        if (index > 0) {
            const StrokePoint &previous = input.points[index - 1];
            const Types::Scalar dx = sample.position.x - previous.position.x;
            const Types::Scalar dy = sample.position.y - previous.position.y;
            const Types::Scalar dt = sample.time - previous.time;
            arcLength += std::hypot(dx, dy);
            sample.velocity = dt > 0.0 ? std::hypot(dx, dy) / dt : 0.0;
        } else {
            sample.velocity = 0.0;
        }
        sample.arcLength = arcLength;

        curve.samples.push_back(sample);
    }

    return curve;
}
