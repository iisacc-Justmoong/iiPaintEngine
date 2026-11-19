#include "LocalizedBrushMath.h"

#include <cfenv>
#include <cmath>

#ifdef IIPAINTENGINE_COUNT_LOCALIZED_BRUSH_ROOT_SOLVES
void countLocalizedBrushRootSolve() noexcept;
#endif

namespace {
bool normalized(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }

// Original f66/L147: preserve each multiply and add in the recorded Lua order.
double radialTransform(double value, double weight) {
    const double square = value * value;
    const double weighted = weight * square;
    const double base = (1 - weight) + weighted;
    const double power2 = base * base;
    const double power4 = power2 * power2;
    return power4 * value;
}
// Original f67/L148. The original uses its result to normalize the brush radius.
double profile(double radius, double weight) {
    const double radiusSquared = radius * radius;
    const double t = 1 - radialTransform(radiusSquared, weight);
    const double t2 = t * t;
    const double t4 = t2 * t2;
    const double t6 = t4 * t2;
    const double edge = .5 * (1 - t);
    const double mixed = (t6 + edge) * t;
    const double weighted = mixed * (1 - weight);
    return t2 * (weighted + weight);
}
double radiusAtThirtyPercent(double weight) {
#ifdef IIPAINTENGINE_COUNT_LOCALIZED_BRUSH_ROOT_SOLVES
    countLocalizedBrushRootSolve();
#endif
    // Original f68/L149: sixteen bisections and interpolation of the final pair.
    double upperRadius = 1, upperValue = 0, lowerRadius = 0, lowerValue = 1;
    constexpr double target = .3;
    for (int i = 0; i < 16; ++i) {
        const double middle = .5 * (upperRadius + lowerRadius);
        const double value = profile(middle, weight);
        if (target <= value) { lowerRadius = middle; lowerValue = value; }
        else { upperRadius = middle; upperValue = value; }
    }
    const double distance = lowerRadius - upperRadius;
    const double delta = target - upperValue;
    const double numerator = distance * delta;
    const double denominator = lowerValue - upperValue;
    return upperRadius + numerator / denominator;
}

// 반경과 독립적인 정규화 결과를 함께 보관한다.
struct FeatherNormalization {
    double root = 0;
    double centerWeight = 0;
};

// 검증이 끝난 feather를 원래 연산 순서로 변환한다.
FeatherNormalization computeFeatherNormalization(double feather)
{
    double weight = 1 - feather;
    for (int i = 0; i < 3; ++i) weight = (weight * .5) * (1 + weight); // Original f70.
    return {radiusAtThirtyPercent(weight), weight};
}

// 같은 스레드의 마지막 feather만 기억한다. 반올림 모드가 바뀌면 새 모드에서 계산한다.
FeatherNormalization normalizationForFeather(double feather)
{
    const int rounding = std::fegetround();
    if (rounding < 0) return computeFeatherNormalization(feather);

    struct CachedFeather {
        double feather = -1;
        int rounding = -1;
        FeatherNormalization normalization;
    };
    static thread_local CachedFeather cached{};
    if (cached.feather != feather || cached.rounding != rounding) {
        cached = {feather, rounding, computeFeatherNormalization(feather)};
    }
    return cached.normalization;
}
} // namespace

std::expected<LocalizedBrushDabParameters, LocalizedBrushMathError>
localizedBrushDabParameters(double radius, double feather) {
    if (!std::isfinite(radius) || radius < 0)
        return std::unexpected(LocalizedBrushMathError::InvalidRadius);
    if (!normalized(feather)) return std::unexpected(LocalizedBrushMathError::InvalidFeather);
    const auto [root, weight] = normalizationForFeather(feather);
    const double result = radius / root; // Original f71. Preserve division after warm lookups.
    if (!std::isfinite(result)) return std::unexpected(LocalizedBrushMathError::NonFiniteResult);
    return LocalizedBrushDabParameters{result, weight};
}

std::expected<double, LocalizedBrushMathError>
localizedBrushProfile(double normalizedRadius, double centerWeight) {
    if (!normalized(normalizedRadius)) return std::unexpected(LocalizedBrushMathError::InvalidRadius);
    if (!normalized(centerWeight)) return std::unexpected(LocalizedBrushMathError::InvalidCenterWeight);
    return profile(normalizedRadius, centerWeight);
}
