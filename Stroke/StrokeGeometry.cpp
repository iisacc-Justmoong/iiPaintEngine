//
// Created by Justmoong on 2026 Jun 07.
//

#include "StrokeGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

constexpr Types::Scalar kCurvatureEpsilon = 0.000001;

Types::Scalar pi()
{
    return std::acos(-1.0);
}

Types::Scalar distance(DocumentPoint lhs, DocumentPoint rhs)
{
    return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

Types::Scalar pointTiltMagnitude(const StrokePoint &point)
{
    return std::hypot(point.tiltX, point.tiltY);
}

Types::Scalar headingForDelta(Types::Scalar dx, Types::Scalar dy)
{
    if (dx == 0.0 && dy == 0.0) {
        return 0.0;
    }
    return std::atan2(dy, dx);
}

Types::Scalar signedTurnRadians(DocumentPoint previous,
                                DocumentPoint current,
                                DocumentPoint next)
{
    const Types::Scalar ax = current.x - previous.x;
    const Types::Scalar ay = current.y - previous.y;
    const Types::Scalar bx = next.x - current.x;
    const Types::Scalar by = next.y - current.y;
    const Types::Scalar aLength = std::hypot(ax, ay);
    const Types::Scalar bLength = std::hypot(bx, by);
    if (aLength <= 0.0 || bLength <= 0.0) {
        return 0.0;
    }

    const Types::Scalar cross = ax * by - ay * bx;
    const Types::Scalar dot = ax * bx + ay * by;
    return std::atan2(cross, dot);
}

DocumentRect boundsForPoints(DocumentPoint lhs, DocumentPoint rhs)
{
    const Types::Scalar left = std::min(lhs.x, rhs.x);
    const Types::Scalar top = std::min(lhs.y, rhs.y);
    const Types::Scalar right = std::max(lhs.x, rhs.x);
    const Types::Scalar bottom = std::max(lhs.y, rhs.y);
    return DocumentRect{{left, top}, right - left, bottom - top};
}

void includeStatisticValue(StrokeScalarStatistics &statistics,
                           Types::Scalar value,
                           Types::Scalar &sum,
                           std::size_t &count)
{
    if (!statistics.valid) {
        statistics.valid = true;
        statistics.minimum = value;
        statistics.maximum = value;
    } else {
        statistics.minimum = std::min(statistics.minimum, value);
        statistics.maximum = std::max(statistics.maximum, value);
    }

    sum += value;
    ++count;
    statistics.average = sum / static_cast<Types::Scalar>(count);
}

Types::Scalar shoelaceSignedArea(const std::vector<StrokePoint> &points)
{
    if (points.size() < 3) {
        return 0.0;
    }

    Types::Scalar doubledArea = 0.0;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const DocumentPoint current = points[index].position;
        const DocumentPoint next = points[(index + 1) % points.size()].position;
        doubledArea += current.x * next.y - current.y * next.x;
    }
    return doubledArea * 0.5;
}

Types::Scalar normalizedAxisRadians(Types::Scalar radians)
{
    while (radians < 0.0) {
        radians += pi();
    }
    while (radians >= pi()) {
        radians -= pi();
    }
    return radians;
}

void describePrincipalAxes(StrokeGeometryReport &report, const std::vector<StrokePoint> &points)
{
    if (points.empty()) {
        return;
    }

    Types::Scalar xx = 0.0;
    Types::Scalar yy = 0.0;
    Types::Scalar xy = 0.0;
    for (const StrokePoint &point : points) {
        const Types::Scalar dx = point.position.x - report.centroid.x;
        const Types::Scalar dy = point.position.y - report.centroid.y;
        xx += dx * dx;
        yy += dy * dy;
        xy += dx * dy;
    }

    const Types::Scalar count = static_cast<Types::Scalar>(points.size());
    xx /= count;
    yy /= count;
    xy /= count;

    report.principalAxisRadians = normalizedAxisRadians(0.5 * std::atan2(2.0 * xy, xx - yy));
    report.secondaryAxisRadians = normalizedAxisRadians(report.principalAxisRadians + pi() * 0.5);

    const Types::Scalar trace = xx + yy;
    const Types::Scalar discriminant = std::sqrt(std::max<Types::Scalar>(
            0.0,
            (xx - yy) * (xx - yy) + 4.0 * xy * xy));
    const Types::Scalar majorVariance = std::max<Types::Scalar>(0.0, (trace + discriminant) * 0.5);
    const Types::Scalar minorVariance = std::max<Types::Scalar>(0.0, (trace - discriminant) * 0.5);
    report.majorSpread = std::sqrt(majorVariance);
    report.minorSpread = std::sqrt(minorVariance);
    report.elongation = report.minorSpread > 0.0
            ? report.majorSpread / report.minorSpread
            : report.majorSpread > 0.0 ? std::numeric_limits<Types::Scalar>::infinity() : 0.0;
}

void describeBoundsAndCentroid(StrokeGeometryReport &report, const std::vector<StrokePoint> &points)
{
    Types::Scalar left = points.front().position.x;
    Types::Scalar top = points.front().position.y;
    Types::Scalar right = points.front().position.x;
    Types::Scalar bottom = points.front().position.y;
    Types::Scalar centroidX = 0.0;
    Types::Scalar centroidY = 0.0;
    Types::Scalar pressureSum = 0.0;
    Types::Scalar tiltSum = 0.0;
    std::size_t pressureCount = 0;
    std::size_t tiltCount = 0;

    for (const StrokePoint &point : points) {
        left = std::min(left, point.position.x);
        top = std::min(top, point.position.y);
        right = std::max(right, point.position.x);
        bottom = std::max(bottom, point.position.y);
        centroidX += point.position.x;
        centroidY += point.position.y;
        includeStatisticValue(report.pressure, point.pressure, pressureSum, pressureCount);
        includeStatisticValue(report.tiltMagnitude, pointTiltMagnitude(point), tiltSum, tiltCount);
    }

    const Types::Scalar sampleCount = static_cast<Types::Scalar>(points.size());
    report.centroid = {centroidX / sampleCount, centroidY / sampleCount};
    report.bounds = {{left, top}, right - left, bottom - top};
}

void describeSegments(StrokeGeometryReport &report, const std::vector<StrokePoint> &points)
{
    Types::Scalar velocitySum = 0.0;
    std::size_t velocityCount = 0;
    Types::Scalar weightedCentroidX = 0.0;
    Types::Scalar weightedCentroidY = 0.0;

    report.segments.reserve(points.size() > 1 ? points.size() - 1 : 0);
    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const StrokePoint &start = points[index];
        const StrokePoint &end = points[index + 1];
        StrokeGeometrySegment segment;
        segment.startIndex = index;
        segment.endIndex = index + 1;
        segment.start = start.position;
        segment.end = end.position;
        segment.midpoint = {
                (start.position.x + end.position.x) * 0.5,
                (start.position.y + end.position.y) * 0.5,
        };
        segment.delta = {
                end.position.x - start.position.x,
                end.position.y - start.position.y,
        };
        segment.bounds = boundsForPoints(start.position, end.position);
        segment.startTime = start.time;
        segment.endTime = end.time;
        segment.startArcLength = report.pathLength;
        segment.length = std::hypot(segment.delta.x, segment.delta.y);
        report.pathLength += segment.length;
        segment.endArcLength = report.pathLength;
        segment.duration = end.time - start.time;
        segment.velocity = segment.duration > 0.0 ? segment.length / segment.duration : 0.0;
        segment.headingRadians = headingForDelta(segment.delta.x, segment.delta.y);
        segment.normalRadians = segment.headingRadians + pi() * 0.5;
        segment.pressureDelta = end.pressure - start.pressure;
        segment.pressurePerDistance = segment.length > 0.0 ? segment.pressureDelta / segment.length : 0.0;
        segment.tiltDelta = pointTiltMagnitude(end) - pointTiltMagnitude(start);

        weightedCentroidX += segment.midpoint.x * segment.length;
        weightedCentroidY += segment.midpoint.y * segment.length;
        includeStatisticValue(report.velocity, segment.velocity, velocitySum, velocityCount);
        report.segments.push_back(segment);
    }

    if (report.pathLength > 0.0) {
        report.lengthWeightedCentroid = {
                weightedCentroidX / report.pathLength,
                weightedCentroidY / report.pathLength,
        };
    } else {
        report.lengthWeightedCentroid = report.centroid;
    }

    for (StrokeGeometrySegment &segment : report.segments) {
        segment.normalizedStartArcLength = report.pathLength > 0.0
                ? segment.startArcLength / report.pathLength
                : 0.0;
        segment.normalizedEndArcLength = report.pathLength > 0.0
                ? segment.endArcLength / report.pathLength
                : 0.0;
    }
}

void describeSamples(StrokeGeometryReport &report, const std::vector<StrokePoint> &points)
{
    report.samples.reserve(points.size());
    Types::Scalar cumulativeTurn = 0.0;
    Types::Scalar curvatureSum = 0.0;
    std::size_t curvatureCount = 0;
    int previousTurnSign = 0;

    for (std::size_t index = 0; index < points.size(); ++index) {
        const StrokePoint &point = points[index];
        StrokeGeometrySample sample;
        sample.sourceIndex = index;
        sample.position = point.position;
        sample.time = point.time;
        sample.pressure = point.pressure;
        sample.arcLength = index == 0 ? 0.0 : report.segments[index - 1].endArcLength;
        sample.normalizedArcLength = report.pathLength > 0.0 ? sample.arcLength / report.pathLength : 0.0;
        sample.incomingHeadingRadians = index > 0
                ? report.segments[index - 1].headingRadians
                : report.segments.empty() ? 0.0 : report.segments.front().headingRadians;
        sample.outgoingHeadingRadians = index + 1 < points.size()
                ? report.segments[index].headingRadians
                : report.segments.empty() ? 0.0 : report.segments.back().headingRadians;

        if (index > 0 && index + 1 < points.size()) {
            sample.tangentRadians = headingForDelta(points[index + 1].position.x - points[index - 1].position.x,
                                                    points[index + 1].position.y - points[index - 1].position.y);
            sample.signedTurnRadians = signedTurnRadians(points[index - 1].position,
                                                         point.position,
                                                         points[index + 1].position);
            const Types::Scalar previousLength = report.segments[index - 1].length;
            const Types::Scalar nextLength = report.segments[index].length;
            const Types::Scalar averageAdjacentLength = (previousLength + nextLength) * 0.5;
            sample.signedCurvature = averageAdjacentLength > 0.0
                    ? sample.signedTurnRadians / averageAdjacentLength
                    : 0.0;
            sample.curvatureRadius = std::abs(sample.signedCurvature) > kCurvatureEpsilon
                    ? 1.0 / std::abs(sample.signedCurvature)
                    : 0.0;
            sample.pressureDerivative = averageAdjacentLength > 0.0
                    ? (points[index + 1].pressure - points[index - 1].pressure)
                            / (previousLength + nextLength)
                    : 0.0;
        } else {
            sample.tangentRadians = index == 0
                    ? sample.outgoingHeadingRadians
                    : sample.incomingHeadingRadians;
            sample.pressureDerivative = index + 1 < points.size()
                    ? report.segments[index].pressurePerDistance
                    : index > 0 ? report.segments[index - 1].pressurePerDistance : 0.0;
        }

        sample.normalRadians = sample.tangentRadians + pi() * 0.5;
        cumulativeTurn += sample.signedTurnRadians;
        sample.cumulativeSignedTurnRadians = cumulativeTurn;
        report.totalSignedTurnRadians += sample.signedTurnRadians;
        report.totalAbsoluteTurnRadians += std::abs(sample.signedTurnRadians);

        const int turnSign = sample.signedTurnRadians > kCurvatureEpsilon
                ? 1
                : sample.signedTurnRadians < -kCurvatureEpsilon ? -1 : 0;
        if (turnSign != 0) {
            if (previousTurnSign != 0 && previousTurnSign != turnSign) {
                ++report.inflectionCount;
            }
            previousTurnSign = turnSign;
        }
        if (std::abs(sample.signedTurnRadians) >= pi() * 0.5) {
            ++report.cuspCount;
        }
        if (std::abs(sample.signedCurvature) > kCurvatureEpsilon) {
            if (curvatureCount == 0) {
                report.minimumSignedCurvature = sample.signedCurvature;
                report.maximumSignedCurvature = sample.signedCurvature;
            } else {
                report.minimumSignedCurvature = std::min(report.minimumSignedCurvature, sample.signedCurvature);
                report.maximumSignedCurvature = std::max(report.maximumSignedCurvature, sample.signedCurvature);
            }
            curvatureSum += std::abs(sample.signedCurvature);
            ++curvatureCount;
        }

        if (report.segments.empty()) {
            sample.velocity = point.velocity;
        } else if (index == 0) {
            sample.velocity = report.segments.front().velocity;
        } else if (index >= report.segments.size()) {
            sample.velocity = report.segments.back().velocity;
        } else {
            sample.velocity = (report.segments[index - 1].velocity + report.segments[index].velocity) * 0.5;
        }

        if (index > 0 && index + 1 < points.size()) {
            const Types::Scalar dt = points[index + 1].time - points[index - 1].time;
            const Types::Scalar incomingVelocity = report.segments[index - 1].velocity;
            const Types::Scalar outgoingVelocity = report.segments[index].velocity;
            sample.acceleration = dt > 0.0 ? (outgoingVelocity - incomingVelocity) / dt : 0.0;
        }

        sample.tiltMagnitude = pointTiltMagnitude(point);
        sample.tiltRadians = headingForDelta(point.tiltX, point.tiltY);
        sample.rotationRadians = point.rotationRadians;
        report.samples.push_back(sample);
    }

    report.averageAbsoluteCurvature = curvatureCount > 0
            ? curvatureSum / static_cast<Types::Scalar>(curvatureCount)
            : 0.0;
}

} // namespace

StrokeGeometryReport describeStrokeGeometry(const StrokeInput &input)
{
    StrokeGeometryReport report;
    report.sourcePointCount = input.points.size();
    report.sampleCount = input.points.size();
    report.segmentCount = input.points.size() > 1 ? input.points.size() - 1 : 0;
    if (input.points.empty()) {
        return report;
    }

    report.empty = false;
    report.start = input.points.front().position;
    report.end = input.points.back().position;
    report.duration = input.points.back().time - input.points.front().time;
    report.chordLength = distance(report.start, report.end);
    report.signedArea = shoelaceSignedArea(input.points);
    report.absoluteArea = std::abs(report.signedArea);

    describeBoundsAndCentroid(report, input.points);
    describeSegments(report, input.points);
    report.straightness = report.pathLength > 0.0 ? report.chordLength / report.pathLength : 0.0;
    report.closedPerimeter = report.pathLength + report.chordLength;
    describeSamples(report, input.points);
    describePrincipalAxes(report, input.points);
    return report;
}

StrokeGeometryReport describeStrokeGeometry(const StrokeCurve &curve)
{
    return describeStrokeGeometry(StrokeInput{curve.samples});
}

StrokeGeometryReport describeStrokeGeometry(const Stroke &stroke)
{
    return describeStrokeGeometry(StrokeInput{stroke.points});
}
