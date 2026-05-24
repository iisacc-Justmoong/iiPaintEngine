//
// Created by Justmoong on 2026 May 24.
//

#include "Stabilizer.h"

#include <algorithm>

StrokeInput stabilizeStrokeInput(const StrokeInput &input, const Stabilizer &stabilizer)
{
    StrokeInput output = input;
    if (input.points.size() < 3) {
        return output;
    }

    const Types::Scalar smoothing = std::clamp(stabilizer.smoothing, 0.0, 1.0);
    if (smoothing == 0.0) {
        return output;
    }

    for (std::size_t index = 1; index + 1 < input.points.size(); ++index) {
        const StrokePoint &previous = input.points[index - 1];
        const StrokePoint &current = input.points[index];
        const StrokePoint &next = input.points[index + 1];

        const CanvasPoint averagedPosition{
                (previous.position.x + current.position.x + next.position.x) / 3.0,
                (previous.position.y + current.position.y + next.position.y) / 3.0,
        };
        const Types::Scalar averagedPressure = (previous.pressure + current.pressure + next.pressure) / 3.0;

        output.points[index].position.x = current.position.x * (1.0 - smoothing) + averagedPosition.x * smoothing;
        output.points[index].position.y = current.position.y * (1.0 - smoothing) + averagedPosition.y * smoothing;
        output.points[index].pressure = current.pressure * (1.0 - smoothing) + averagedPressure * smoothing;
    }

    return output;
}
