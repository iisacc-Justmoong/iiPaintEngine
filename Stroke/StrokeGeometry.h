//
// Created by Justmoong on 2026 Jun 07.
//

#pragma once

#include <cstddef>
#include <vector>

#include "Core/PaintPoint.h"
#include "Core/PaintRect.h"
#include "Core/Types.h"
#include "Stroke/Stroke.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeInput.h"

struct StrokeScalarStatistics {
    bool valid = false;
    Types::Scalar minimum = 0.0;
    Types::Scalar maximum = 0.0;
    Types::Scalar average = 0.0;
};

struct StrokeGeometrySegment {
    std::size_t startIndex = 0;
    std::size_t endIndex = 0;
    DocumentPoint start{};
    DocumentPoint end{};
    DocumentPoint midpoint{};
    DocumentPoint delta{};
    DocumentRect bounds{};
    Types::Scalar startTime = 0.0;
    Types::Scalar endTime = 0.0;
    Types::Scalar startArcLength = 0.0;
    Types::Scalar endArcLength = 0.0;
    Types::Scalar normalizedStartArcLength = 0.0;
    Types::Scalar normalizedEndArcLength = 0.0;
    Types::Scalar length = 0.0;
    Types::Scalar duration = 0.0;
    Types::Scalar velocity = 0.0;
    Types::Scalar headingRadians = 0.0;
    Types::Scalar normalRadians = 0.0;
    Types::Scalar pressureDelta = 0.0;
    Types::Scalar pressurePerDistance = 0.0;
    Types::Scalar tiltDelta = 0.0;
};

struct StrokeGeometrySample {
    std::size_t sourceIndex = 0;
    DocumentPoint position{};
    Types::Scalar time = 0.0;
    Types::Scalar pressure = 0.0;
    Types::Scalar arcLength = 0.0;
    Types::Scalar normalizedArcLength = 0.0;
    Types::Scalar incomingHeadingRadians = 0.0;
    Types::Scalar outgoingHeadingRadians = 0.0;
    Types::Scalar tangentRadians = 0.0;
    Types::Scalar normalRadians = 0.0;
    Types::Scalar signedTurnRadians = 0.0;
    Types::Scalar cumulativeSignedTurnRadians = 0.0;
    Types::Scalar signedCurvature = 0.0;
    Types::Scalar curvatureRadius = 0.0;
    Types::Scalar velocity = 0.0;
    Types::Scalar acceleration = 0.0;
    Types::Scalar pressureDerivative = 0.0;
    Types::Scalar tiltMagnitude = 0.0;
    Types::Scalar tiltRadians = 0.0;
    Types::Scalar rotationRadians = 0.0;
};

struct StrokeGeometryReport {
    bool empty = true;
    std::size_t sourcePointCount = 0;
    std::size_t sampleCount = 0;
    std::size_t segmentCount = 0;
    DocumentPoint start{};
    DocumentPoint end{};
    DocumentPoint centroid{};
    DocumentPoint lengthWeightedCentroid{};
    DocumentRect bounds{};
    Types::Scalar pathLength = 0.0;
    Types::Scalar chordLength = 0.0;
    Types::Scalar duration = 0.0;
    Types::Scalar straightness = 0.0;
    Types::Scalar signedArea = 0.0;
    Types::Scalar absoluteArea = 0.0;
    Types::Scalar closedPerimeter = 0.0;
    Types::Scalar totalSignedTurnRadians = 0.0;
    Types::Scalar totalAbsoluteTurnRadians = 0.0;
    Types::Scalar minimumSignedCurvature = 0.0;
    Types::Scalar maximumSignedCurvature = 0.0;
    Types::Scalar averageAbsoluteCurvature = 0.0;
    std::size_t inflectionCount = 0;
    std::size_t cuspCount = 0;
    StrokeScalarStatistics pressure{};
    StrokeScalarStatistics velocity{};
    StrokeScalarStatistics tiltMagnitude{};
    Types::Scalar principalAxisRadians = 0.0;
    Types::Scalar secondaryAxisRadians = 0.0;
    Types::Scalar majorSpread = 0.0;
    Types::Scalar minorSpread = 0.0;
    Types::Scalar elongation = 0.0;
    std::vector<StrokeGeometrySample> samples;
    std::vector<StrokeGeometrySegment> segments;
};

StrokeGeometryReport describeStrokeGeometry(const StrokeInput &input);

StrokeGeometryReport describeStrokeGeometry(const StrokeCurve &curve);

StrokeGeometryReport describeStrokeGeometry(const Stroke &stroke);
