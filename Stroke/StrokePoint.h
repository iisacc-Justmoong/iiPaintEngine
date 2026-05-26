//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Core/PaintPoint.h"
#include "Core/Types.h"

struct StrokePoint {
    DocumentPoint position;
    Types::Scalar pressure = 1.0;
    Types::Scalar time = 0.0;
    Types::Scalar velocity = 0.0;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    std::uint32_t deviceState = 0;
    Types::Scalar arcLength = 0.0;
    Types::Scalar rotationRadians = 0.0;
};
