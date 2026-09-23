#pragma once

#include <cstdint>
#include "Core/Types.h"
#include "Core/RasterBlendMode.h"

enum class StrokeTaperShape { Linear, EaseIn, EaseOut, SmoothStep };

struct BrushStrokeSettings {
    Types::Scalar spacing = 0.0; // Positive absolute document spacing overrides the ratio.
    Types::Scalar spacingRatio = 0.1;
    bool spacingEnabled = true;
    bool spacingFollowsSize = false;
    bool flowEnabled = true;
    bool opacityEnabled = true;
    bool hardnessEnabled = true;
    Types::Scalar warmupDistance = 0.0;
    Types::Scalar taperMinimum = 0.25;
    StrokeTaperShape warmupTaperShape = StrokeTaperShape::Linear;
    bool airbrushEnabled = false; // Time-based placement instead of distance-based placement.
    Types::Scalar airbrushRate = 30.0; // Dabs per second; caller supplies monotonic seconds.
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
};

struct BrushColorSettings {
    bool enabled = false;
    std::uint32_t secondaryArgb = 0xFFFFFFFFU;
    Types::Scalar mix = 0.0;
    Types::Scalar hueJitter = 0.0; // Fraction of a full hue turn, symmetric about zero.
    Types::Scalar saturationJitter = 0.0;
    Types::Scalar valueJitter = 0.0;
    bool perStroke = false;
};
