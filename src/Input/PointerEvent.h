//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

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
    Barrel,
    Eraser,
};

enum class PointerToolKind {
    Mouse,
    Finger,
    Pen,
    Eraser,
};

enum class TouchGesturePhase {
    None,
    Begin,
    Update,
    End,
    Cancel,
};

inline constexpr std::uint32_t PointerDeviceStatePrimaryButton = 1U << 0U;
inline constexpr std::uint32_t PointerDeviceStateBarrelButton = 1U << 1U;
inline constexpr std::uint32_t PointerDeviceStateEraser = 1U << 2U;
inline constexpr std::uint32_t PointerDeviceStateHover = 1U << 3U;
inline constexpr std::uint32_t PointerDeviceStateTouchGesture = 1U << 4U;

struct PointerEvent {
    PointerDeviceKind device = PointerDeviceKind::Mouse;
    PointerEventPhase phase = PointerEventPhase::Move;
    DocumentPoint documentPosition{};
    Types::Scalar pressure = 1.0;
    Types::Scalar time = 0.0;
    PointerButton button = PointerButton::None;
    bool primaryButtonDown = false;
    Types::Scalar tiltX = 0.0;
    Types::Scalar tiltY = 0.0;
    PointerToolKind tool = PointerToolKind::Mouse;
    bool hovering = false;
    bool barrelButtonDown = false;
    bool eraserActive = false;
    Types::Scalar rotationRadians = 0.0;
    std::uint32_t deviceState = 0;
    TouchGesturePhase gesturePhase = TouchGesturePhase::None;
    DocumentPoint gestureCentroid{};
    DocumentPoint gestureTranslation{};
    Types::Scalar gestureScale = 1.0;
    Types::Scalar gestureRotationRadians = 0.0;
    std::uint32_t touchPointCount = 0;
};
