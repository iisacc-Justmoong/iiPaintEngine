#include "Brush/LocalizedBrushMath.h"
#include "fixtures/LocalizedBrushGolden.h"

#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
int main() {
    try {
        for (const auto &golden : localizedBrushGolden) {
            const auto parameters = localizedBrushDabParameters(golden.radius, golden.feather);
            check(parameters && std::bit_cast<std::uint64_t>(parameters->radius) == golden.adjustedRadiusBits
                    && std::bit_cast<std::uint64_t>(parameters->centerWeight) == golden.centerWeightBits,
                  "parameters must bit-match original unmodified Lua prototypes");
            const auto sampled = localizedBrushProfile(std::bit_cast<double>(golden.rootBits),
                                                       std::bit_cast<double>(golden.centerWeightBits));
            check(sampled && std::bit_cast<std::uint64_t>(*sampled) == golden.profileBits,
                  "profile must bit-match original unmodified Lua prototype");
        }
        for (int i = 0; i <= 100; ++i) {
            const double feather = i / 100.;
            auto parameters = localizedBrushDabParameters(12.5, feather);
            check(bool(parameters) && parameters->radius > 12.5 && parameters->centerWeight >= 0
                  && parameters->centerWeight <= 1, "valid feather produces finite parameters");
            auto atTarget = localizedBrushProfile(12.5 / parameters->radius, parameters->centerWeight);
            check(bool(atTarget) && std::abs(*atTarget - .3) < 3e-8,
                  "sixteen-step interpolated radius lies on thirty-percent profile");
            double previous = 1;
            for (int j = 0; j <= 100; ++j) {
                const auto sample = localizedBrushProfile(j / 100., parameters->centerWeight);
                check(sample && *sample >= 0 && *sample <= 1 && *sample <= previous,
                      "radial profile remains bounded and monotonically decreasing");
                previous = *sample;
            }
        }
        const auto hard = localizedBrushDabParameters(1, 0);
        const auto soft = localizedBrushDabParameters(1, 1);
        check(hard && hard->centerWeight == 1 && soft && soft->centerWeight == 0,
              "feather endpoints retain observed weights");
        const double analyticalHardRadius = std::pow(1 - std::sqrt(.3), 1. / 18.);
        check(std::abs(hard->radius - 1 / analyticalHardRadius) < 3e-9,
              "weight1 root agrees with independent analytical solution");
        const auto scaled = localizedBrushDabParameters(12.5, .5);
        const auto unit = localizedBrushDabParameters(1, .5);
        check(scaled && unit && std::abs(scaled->radius - 12.5 * unit->radius) < 1e-13,
              "parameter radius preserves caller units");
        const auto zero = localizedBrushDabParameters(0, .5);
        check(zero && zero->radius == 0, "zero radius remains zero");
        check(!localizedBrushDabParameters(-1, .5), "negative radius rejected");
        check(!localizedBrushDabParameters(std::numeric_limits<double>::infinity(), .5),
              "nonfinite radius rejected");
        check(!localizedBrushDabParameters(1, std::numeric_limits<double>::quiet_NaN()),
              "nonfinite feather rejected");
        check(!localizedBrushDabParameters(1, 1.01) && !localizedBrushDabParameters(1, -.01),
              "feather range enforced");
        const auto overflow = localizedBrushDabParameters(std::numeric_limits<double>::max(), 1);
        check(!overflow && overflow.error() == LocalizedBrushMathError::NonFiniteResult,
              "derived radius overflow reported");
        check(!localizedBrushProfile(-.1, .5) && !localizedBrushProfile(1.1, .5)
              && !localizedBrushProfile(.5, -1), "profile range enforced");
        std::cout << "Localized brush numeric contracts passed; 21 original-prototype vectors bit-matched\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
