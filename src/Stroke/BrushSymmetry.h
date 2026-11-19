#pragma once
#include "Stroke/Rasterizer.h"

// Stateless expansion of current bitmap stamps; includes each original stamp.
// Equal centers within one stamp's expansion are emitted once. At most 65536
// resulting stamps are allowed per call. Invalid settings throw invalid_argument.
std::vector<BrushDab> applyBrushSymmetry(BrushDabSpan dabs, const BrushSymmetrySettings &settings);

// Applies the caller's dab limit to unique emitted copies and throws std::length_error if exceeded.
std::vector<BrushDab> applyBrushSymmetryWithLimit(BrushDabSpan dabs, const BrushSymmetrySettings &settings,
                                                std::size_t maxDabs);
