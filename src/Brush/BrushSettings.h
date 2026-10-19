#pragma once

#include <cstdint>
#include "Core/Types.h"
#include "Core/PaintPoint.h"
#include "Core/RasterBlendMode.h"

enum class StrokeTaperShape { Linear, EaseIn, EaseOut, SmoothStep };
enum class BrushSmoothingMode { Exponential, PulledString };
enum class BrushSymmetryMode { None, Vertical, Horizontal, DualAxis, Radial, Mandala };

struct BrushSymmetrySettings {
    BrushSymmetryMode mode = BrushSymmetryMode::None;
    DocumentPoint center{};
    Types::Scalar angleRadians = 0.0; // Rotates the reflection axes.
    std::uint32_t segments = 6; // Radial: 2..64 copies; Mandala: 2..64 mirrored sectors.
};

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
    bool noiseEnabled = false;
    Types::Scalar noiseStrength = 0.5;
    bool wetEdgesEnabled = false;
    Types::Scalar wetEdgesStrength = 0.5;
    bool smoothingEnabled = false;
    Types::Scalar smoothing = 0.5; // Causal position filter; never retains a point list.
    bool protectTexture = false; // Preserve pattern and scale when selecting another preset.
    BrushSmoothingMode smoothingMode = BrushSmoothingMode::Exponential;
    Types::Scalar smoothingRadius = 10.0; // Document pixels, multiplied by smoothing amount.
    bool smoothingCatchUp = false; // PulledString only; caller advances the clock.
    Types::Scalar smoothingCatchUpRate = 12.0; // Inverse seconds.
    bool smoothingCatchUpOnEnd = false;
    BrushSymmetrySettings symmetry;
};

struct BrushColorSettings {
    bool enabled = false;
    std::uint32_t secondaryArgb = 0xFFFFFFFFU;
    Types::Scalar mix = 0.0;
    Types::Scalar hueJitter = 0.0; // Fraction of a full hue turn, symmetric about zero.
    Types::Scalar saturationJitter = 0.0;
    Types::Scalar valueJitter = 0.0;
    bool perStroke = false;
    Types::Scalar mixJitter = 0.0;
    Types::Scalar purity = 0.0; // -1 fully desaturated, +1 fully saturated.
};
