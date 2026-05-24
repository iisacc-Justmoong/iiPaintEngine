//
// Created by Justmoong on 2026 May 24.
//

#include "StrokeCurve.h"

StrokeCurve makeStrokeCurve(const StrokeInput &input)
{
    StrokeCurve curve;
    curve.points.reserve(input.points.size());

    for (const StrokePoint &point : input.points) {
        curve.points.push_back(point.position);
    }

    return curve;
}
