//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/Types.h"

enum class BrushTipShape { Round, Square, Diamond };
enum class BrushAngleMode { Automatic, Fixed, StrokeDirection, TiltDirection, StylusRotation };

struct BrushShape {
    BrushTipShape kind = BrushTipShape::Round;
    BrushAngleMode angleMode = BrushAngleMode::Automatic;
    Types::Scalar angleRadians = 0.0;
    Types::Scalar roundness = 1.0;
    bool flipX = false;
    bool flipY = false;
};
