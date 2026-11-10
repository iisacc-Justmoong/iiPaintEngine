#pragma once

#include <expected>

enum class LocalizedBrushMathError { InvalidRadius, InvalidFeather, InvalidCenterWeight, NonFiniteResult };

struct LocalizedBrushDabParameters {
    double radius = 0; // Same units as caller-supplied radius.
    double centerWeight = 0;
};

// Reconstructed pure numeric helpers from Lightroom9.6's shipped Lua bytecode.
// Radius is finite and nonnegative; feather is normalized [0,1]. This converts
// parameters only, without claiming the CameraRaw native pixel kernel.
[[nodiscard]] std::expected<LocalizedBrushDabParameters, LocalizedBrushMathError>
localizedBrushDabParameters(double radius, double feather);

// The helper profile used by the original parameter converter's0.3 root search.
// Both arguments are finite [0,1]. This is not an image or native brush renderer.
[[nodiscard]] std::expected<double, LocalizedBrushMathError>
localizedBrushProfile(double normalizedRadius, double centerWeight);
