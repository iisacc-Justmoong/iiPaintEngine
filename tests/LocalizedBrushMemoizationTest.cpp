#include "Brush/LocalizedBrushMath.h"
#include "fixtures/LocalizedBrushGolden.h"

#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t rootSolves = 0;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool sameBits(const LocalizedBrushDabParameters& a, const LocalizedBrushDabParameters& b)
{
    return std::bit_cast<std::uint64_t>(a.radius) == std::bit_cast<std::uint64_t>(b.radius)
        && std::bit_cast<std::uint64_t>(a.centerWeight) == std::bit_cast<std::uint64_t>(b.centerWeight);
}

// 같은 feather로 반경만 바꾸면서 원본 Lua 프로토타입의 골든과 실제 탐색 수를 비교한다.
void repeatedFeather()
{
    constexpr int repeats = 128;
    std::size_t groups = 0;
    auto first = localizedBrushGolden.begin();
    while (first != localizedBrushGolden.end()) {
        const auto feather = first->feather;
        const auto end = std::find_if(first, localizedBrushGolden.end(), [feather](const auto& golden) {
            return golden.feather != feather;
        });
        ++groups;
        for (int repeat = 0; repeat < repeats; ++repeat) {
            for (auto entry = first; entry != end; ++entry) {
                const auto actual = localizedBrushDabParameters(entry->radius, feather);
                const LocalizedBrushDabParameters expected{std::bit_cast<double>(entry->adjustedRadiusBits),
                                                           std::bit_cast<double>(entry->centerWeightBits)};
                check(actual && sameBits(*actual, expected), "memoized parameters differ from the original golden");
            }
        }
        first = end;
    }
    std::cout << "calls=" << localizedBrushGolden.size() * repeats << " root_solves=" << rootSolves
              << " feather_groups=" << groups << '\n';
    check(rootSolves == groups, "same-feather dab calls repeated the bisection");
}

// 한 ULP가 다른 feather도 별도 계산하며, 잘못된 반경은 유효한 캐시를 덮지 않는다.
void cacheKeysAndErrors()
{
    constexpr double feather = .5;
    const auto adjacent = std::nextafter(feather, 1.);
    const auto before = rootSolves;
    const auto first = localizedBrushDabParameters(1, feather);
    const auto other = localizedBrushDabParameters(1, adjacent);
    const auto repeated = localizedBrushDabParameters(1, feather);
    check(first && other && repeated && sameBits(*first, *repeated) && rootSolves - before == 3,
          "feather changes did not replace the single cached value");

    const auto invalidRadius = localizedBrushDabParameters(-1, adjacent);
    const auto invalidFeather = localizedBrushDabParameters(1, std::numeric_limits<double>::quiet_NaN());
    const auto overflow = localizedBrushDabParameters(std::numeric_limits<double>::max(), feather);
    const auto negativeZero = localizedBrushDabParameters(-0., feather);
    check(!invalidRadius && invalidRadius.error() == LocalizedBrushMathError::InvalidRadius
          && !invalidFeather && invalidFeather.error() == LocalizedBrushMathError::InvalidFeather
          && !overflow && overflow.error() == LocalizedBrushMathError::NonFiniteResult
          && negativeZero && std::bit_cast<std::uint64_t>(negativeZero->radius) == std::bit_cast<std::uint64_t>(-0.)
          && rootSolves - before == 3, "validation or overflow changed the cached feather");
}

// 지원되는 반올림 모드가 바뀌면 같은 feather의 정규화도 다시 계산한다.
void roundingMode()
{
#ifdef FE_DOWNWARD
    const int initial = std::fegetround();
    if (initial < 0 || initial == FE_DOWNWARD) return;
    const auto original = localizedBrushDabParameters(12.5, .5);
    if (std::fesetround(FE_DOWNWARD) != 0) return;
    struct RestoreRounding {
        int mode;
        ~RestoreRounding() { std::fesetround(mode); }
    } restore{initial};

    const auto before = rootSolves;
    const auto changed = localizedBrushDabParameters(12.5, .5);
    const auto hit = localizedBrushDabParameters(12.5, .5);
    check(changed && hit && sameBits(*changed, *hit) && rootSolves - before == 1,
          "rounding mode did not distinguish two feather calculations");
    check(localizedBrushDabParameters(1, 1).has_value(), "rounding fixture did not evict the cache entry");
    const auto cold = localizedBrushDabParameters(12.5, .5);
    check(cold && sameBits(*changed, *cold), "rounding mode cache missed the fresh result");

    check(std::fesetround(initial) == 0, "rounding mode restore failed");
    const auto restored = localizedBrushDabParameters(12.5, .5);
    check(original && restored && sameBits(*original, *restored) && rootSolves - before == 4,
          "original rounding mode did not restore the original numeric result");
#endif
}
} // namespace

// 실제 16단계 탐색에 진입했을 때만 테스트 계측을 증가시킨다.
void countLocalizedBrushRootSolve() noexcept { ++rootSolves; }

int main()
{
    try {
        repeatedFeather();
        cacheKeysAndErrors();
        roundingMode();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
