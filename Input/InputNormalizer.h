//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>

#include "Input/PointerEvent.h"
#include "Input/TabletState.h"

struct TouchGestureState {
    TouchGesturePhase phase = TouchGesturePhase::None;
    DocumentPoint centroid{};
    DocumentPoint translation{};
    Types::Scalar scale = 1.0;
    Types::Scalar rotationRadians = 0.0;
    std::uint32_t fingerCount = 0;
    Types::Scalar time = 0.0;
};

struct InputNormalizer {
    bool pressureEnabled = true;
    bool tiltEnabled = true;
    bool rotationEnabled = true;
    bool hoverEnabled = true;
    bool barrelButtonEnabled = true;
    bool eraserEnabled = true;
    bool touchGestureEnabled = true;
    Types::Scalar pressureMin = 0.0;
    Types::Scalar pressureMax = 1.0;
    TabletTiltCalibration tabletTiltCalibration;
};

PointerEvent normalizeTabletPointerEvent(const InputNormalizer &normalizer,
                                         const TabletState &tablet,
                                         PointerEventPhase phase);

PointerEvent normalizeTouchGesturePointerEvent(const InputNormalizer &normalizer,
                                               const TouchGestureState &gesture);
