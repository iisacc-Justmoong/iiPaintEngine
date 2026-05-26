//
// Created by Justmoong on 2026 May 24.
//

#include "Stabilizer.h"

#include <algorithm>
#include <cmath>

namespace {

Types::Scalar clamp01(Types::Scalar value)
{
    return std::clamp(value, 0.0, 1.0);
}

Types::Scalar distance(CanvasPoint lhs, CanvasPoint rhs)
{
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

Types::Scalar segmentVelocity(const StrokePoint &start, const StrokePoint &end)
{
    const Types::Scalar dt = end.time - start.time;
    if (dt > 0.0) {
        return distance(start.position, end.position) / dt;
    }
    return std::max(start.velocity, end.velocity);
}

Types::Scalar turnAngleRadians(const StrokePoint &previous,
                               const StrokePoint &current,
                               const StrokePoint &next)
{
    const Types::Scalar ax = current.position.x - previous.position.x;
    const Types::Scalar ay = current.position.y - previous.position.y;
    const Types::Scalar bx = next.position.x - current.position.x;
    const Types::Scalar by = next.position.y - current.position.y;
    const Types::Scalar aLength = std::hypot(ax, ay);
    const Types::Scalar bLength = std::hypot(bx, by);
    if (aLength <= 0.0 || bLength <= 0.0) {
        return 0.0;
    }

    const Types::Scalar cosine = std::clamp((ax * bx + ay * by) / (aLength * bLength), -1.0, 1.0);
    return std::acos(cosine);
}

bool preservesCusp(const StrokeInput &input, const Stabilizer &stabilizer, std::size_t index)
{
    return stabilizer.cuspPreservationEnabled
            && index > 0
            && index + 1 < input.points.size()
            && turnAngleRadians(input.points[index - 1], input.points[index], input.points[index + 1])
                    >= std::max<Types::Scalar>(0.0, stabilizer.cuspAngleRadians);
}

CanvasPoint projectionOntoLine(CanvasPoint point, CanvasPoint start, CanvasPoint end)
{
    const Types::Scalar dx = end.x - start.x;
    const Types::Scalar dy = end.y - start.y;
    const Types::Scalar lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 0.0) {
        return point;
    }

    const Types::Scalar t = std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared,
                                       0.0,
                                       1.0);
    return CanvasPoint{start.x + dx * t, start.y + dy * t};
}

StrokePoint interpolateStrokePoint(const StrokePoint &start, const StrokePoint &end, Types::Scalar t)
{
    StrokePoint sample;
    sample.position = {
            start.position.x + (end.position.x - start.position.x) * t,
            start.position.y + (end.position.y - start.position.y) * t,
    };
    sample.pressure = start.pressure + (end.pressure - start.pressure) * t;
    sample.time = start.time + (end.time - start.time) * t;
    sample.velocity = start.velocity + (end.velocity - start.velocity) * t;
    sample.tiltX = start.tiltX + (end.tiltX - start.tiltX) * t;
    sample.tiltY = start.tiltY + (end.tiltY - start.tiltY) * t;
    sample.deviceState = t < 1.0 ? start.deviceState : end.deviceState;
    sample.rotationRadians = start.rotationRadians + (end.rotationRadians - start.rotationRadians) * t;
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
    sample.velocity = dt > 0.0 ? segmentLength / dt : sample.velocity;
    output.points.push_back(sample);
}

void applySmoothing(StrokeInput &output, const StrokeInput &input, const Stabilizer &stabilizer)
{
    if (input.points.size() < 3) {
        return;
    }

    const Types::Scalar smoothing = clamp01(stabilizer.smoothing);
    if (smoothing == 0.0) {
        return;
    }

    for (std::size_t index = 1; index + 1 < input.points.size(); ++index) {
        if (preservesCusp(input, stabilizer, index)) {
            continue;
        }

        const StrokePoint &previous = input.points[index - 1];
        const StrokePoint &current = input.points[index];
        const StrokePoint &next = input.points[index + 1];

        CanvasPoint targetPosition{
                (previous.position.x + current.position.x + next.position.x) / 3.0,
                (previous.position.y + current.position.y + next.position.y) / 3.0,
        };
        if (stabilizer.mode == StabilizerMode::Line) {
            targetPosition = projectionOntoLine(current.position, previous.position, next.position);
        }

        const Types::Scalar averagedPressure = (previous.pressure + current.pressure + next.pressure) / 3.0;
        output.points[index].position.x = current.position.x * (1.0 - smoothing) + targetPosition.x * smoothing;
        output.points[index].position.y = current.position.y * (1.0 - smoothing) + targetPosition.y * smoothing;
        output.points[index].pressure = current.pressure * (1.0 - smoothing) + averagedPressure * smoothing;
    }
}

void applyPrediction(StrokeInput &output, const Stabilizer &stabilizer)
{
    if (!stabilizer.predictionEnabled || output.points.size() < 2) {
        return;
    }

    const Types::Scalar predictionTime = std::max<Types::Scalar>(
            0.0,
            stabilizer.predictionHorizon + stabilizer.latencyCompensation);
    if (predictionTime <= 0.0) {
        return;
    }

    const StrokePoint &previous = output.points[output.points.size() - 2];
    StrokePoint &last = output.points.back();
    const Types::Scalar dt = last.time - previous.time;
    if (dt <= 0.0) {
        return;
    }

    const Types::Scalar vx = (last.position.x - previous.position.x) / dt;
    const Types::Scalar vy = (last.position.y - previous.position.y) / dt;
    last.position.x += vx * predictionTime;
    last.position.y += vy * predictionTime;
    last.time += predictionTime;
    last.velocity = std::hypot(vx, vy);
    last.arcLength = 0.0;
}

StrokeInput adaptiveResampleStrokeInput(const StrokeInput &input, const Stabilizer &stabilizer)
{
    if (!stabilizer.adaptiveResamplingEnabled || input.points.size() < 2) {
        StrokeInput output = input;
        if (!output.points.empty()) {
            output.points.front().arcLength = 0.0;
        }
        return output;
    }

    const Types::Scalar minSpacing = std::max<Types::Scalar>(0.01, stabilizer.adaptiveMinSpacing);
    const Types::Scalar maxSpacing = std::max(minSpacing, stabilizer.adaptiveMaxSpacing);
    const Types::Scalar velocityScale = std::max<Types::Scalar>(0.0, stabilizer.adaptiveVelocityScale);

    StrokeInput output;
    appendWithArcLength(output, input.points.front());

    for (std::size_t index = 0; index + 1 < input.points.size(); ++index) {
        const StrokePoint &start = input.points[index];
        const StrokePoint &end = input.points[index + 1];
        const Types::Scalar segmentLength = distance(start.position, end.position);
        if (segmentLength <= 0.0) {
            continue;
        }

        const Types::Scalar velocity = segmentVelocity(start, end);
        const Types::Scalar spacing = std::clamp(maxSpacing / (1.0 + velocity * velocityScale),
                                                 minSpacing,
                                                 maxSpacing);
        const int steps = std::max(1, static_cast<int>(std::ceil(segmentLength / spacing)));
        for (int step = 1; step <= steps; ++step) {
            const Types::Scalar t = std::clamp(static_cast<Types::Scalar>(step) / static_cast<Types::Scalar>(steps),
                                               0.0,
                                               1.0);
            appendWithArcLength(output, interpolateStrokePoint(start, end, t));
        }
    }

    return output;
}

} // namespace

StrokeInput stabilizeStrokeInput(const StrokeInput &input, const Stabilizer &stabilizer)
{
    StrokeInput output = input;
    if (input.points.empty()) {
        return output;
    }

    applySmoothing(output, input, stabilizer);
    applyPrediction(output, stabilizer);
    return adaptiveResampleStrokeInput(output, stabilizer);
}
