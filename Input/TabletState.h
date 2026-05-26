//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Core/PaintPoint.h"
#include "Core/Types.h"

enum class TabletToolKind {
    Pen,
    Eraser,
};

struct TabletTiltCalibration {
    Types::Scalar offsetX = 0.0;
    Types::Scalar offsetY = 0.0;
    Types::Scalar scaleX = 1.0;
    Types::Scalar scaleY = 1.0;
    Types::Scalar maxTilt = 1.0;
};

struct TabletState {
    DocumentPoint documentPosition{};
    Types::Scalar pressure = 0.0;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar time = 0.0;
    bool inProximity = false;
    bool hovering = false;
    bool contact = false;
    bool primaryButtonDown = false;
    bool barrelButtonDown = false;
    bool eraser = false;
    TabletToolKind tool = TabletToolKind::Pen;
};
