#pragma once
#include "Core/Types.h"

enum class MaskBrushMode { Add, Erase };

// Independent local-correction brush options. BrushPreset::density still means
// dab placement density; this density is an absolute normalized mask ceiling.
struct LocalMaskBrushOptions {
    Types::Scalar sizePixels = 24; // Diameter in document pixels; [0.01,4096].
    Types::Scalar feather = 1; // [0,1], edge transition as a fraction of radius.
    Types::Scalar flow = 1; // [0,1], per-dab rate. Zero leaves pixels untouched.
    Types::Scalar density = 1; // [0,1], absolute Add coverage ceiling; ignored for Erase.
    Types::Scalar strokeOpacity = 1; // [0,1], maximum effect of one entire stroke.
    bool pressureAffectsSize = false; // Existing legacy pressure-to-size model.
    bool pressureAffectsOpacity = false; // Per-dab pending coverage ceiling.
    bool autoMask = false;
    Types::Scalar autoMaskTolerance = 0.15; // [0,1], RMS RGB distance in supplied 8-bit color space.
    MaskBrushMode mode = MaskBrushMode::Add;
};

bool isValidLocalMaskBrushOptions(const LocalMaskBrushOptions &options);
