//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/PaintPoint.h"
#include "Core/Types.h"

enum class PointerDeviceKind {
    Mouse,
    Touch,
    Tablet,
};

enum class PointerEventPhase {
    Press,
    Move,
    Release,
    Cancel,
};

enum class PointerButton {
    None,
    Primary,
    Secondary,
    Middle,
};

struct PointerEvent {
    PointerDeviceKind device = PointerDeviceKind::Mouse;
    PointerEventPhase phase = PointerEventPhase::Move;
    CanvasPoint canvasPosition{};
    Types::Scalar pressure = 1.0;
    Types::Scalar time = 0.0;
    PointerButton button = PointerButton::None;
    bool primaryButtonDown = false;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
};
