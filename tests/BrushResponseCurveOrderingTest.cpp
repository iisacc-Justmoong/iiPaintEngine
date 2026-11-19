#include "Brush/BrushDynamics.h"
#include "Brush/BrushPresetSerializer.h"
#include "Brush/BrushResolve.h"
#include "Stroke/Rasterizer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<typename Function>
void checkRejected(Function run, const char* message)
{
    try { run(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}

// 변경 전 선형 구간 탐색의 판정과 보간 순서로 골든 값을 구한다.
double linearReference(const BrushDynamicsResponseCurve& curve, double input)
{
    const auto t = std::isfinite(input) ? std::clamp(input, 0., 1.) : 0.;
    if (t <= curve.points.front().input) return curve.points.front().output;
    for (std::size_t i = 1; i < curve.points.size(); ++i) {
        const auto& lower = curve.points[i - 1];
        const auto& upper = curve.points[i];
        if (t > upper.input) continue;
        const auto u = std::clamp((t - lower.input) / (upper.input - lower.input), 0., 1.);
        double eased = u;
        switch (curve.easing) {
            case BrushDynamicsEasing::Linear: break;
            case BrushDynamicsEasing::EaseIn: eased = u * u; break;
            case BrushDynamicsEasing::EaseOut: eased = 1 - (1 - u) * (1 - u); break;
            case BrushDynamicsEasing::EaseInOut:
                eased = u < .5 ? 2 * u * u : 1 - 2 * (1 - u) * (1 - u);
                break;
        }
        return lower.output + (upper.output - lower.output) * eased;
    }
    return curve.points.back().output;
}

// 64개 비균일 매듭의 끝값, 정확한 매듭, 양옆 1 ULP, 임의 보간점까지 비교한다.
void exactInterpolation()
{
    BrushDynamicsResponseCurve curve;
    for (int i = 0; i < 64; ++i) {
        curve.points.push_back({(1 + i * i) / 4096., ((i * 97) % 83 - 41) / 32.});
    }
    const auto compare = [&](double t) {
        const auto actual = evaluateBrushResponseCurve(curve, t);
        const auto expected = linearReference(curve, t);
        check(std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected),
              "curve interpolation differs from the linear reference");
    };
    for (const auto easing : {BrushDynamicsEasing::Linear, BrushDynamicsEasing::EaseIn,
                              BrushDynamicsEasing::EaseOut, BrushDynamicsEasing::EaseInOut}) {
        curve.easing = easing;
        for (int i = 0; i <= 4096; ++i) compare(i / 4096.);
        for (const auto& knot : curve.points) {
            compare(knot.input);
            compare(std::nextafter(knot.input, 0.));
            compare(std::nextafter(knot.input, 1.));
        }
        compare(-1);
        compare(2);
        compare(std::numeric_limits<double>::infinity());
        compare(std::numeric_limits<double>::quiet_NaN());
    }

    // 마지막 매듭과 같은 입력도 기존 보간식의 반올림을 따른다.
    curve.points = {{0, 1}, {.75, std::numeric_limits<double>::min()}};
    compare(.75);
    compare(std::nextafter(.75, 1.));

    curve.points = {{.42, -.75}};
    for (const auto t : {0., .42, 1.}) compare(t);
    curve.points.clear();
    curve.easing = BrushDynamicsEasing::Linear;
    curve.min = .2; curve.center = .5; curve.max = .8;
    check(evaluateBrushResponseCurve(curve, 0) == curve.min
          && evaluateBrushResponseCurve(curve, .5) == curve.center
          && evaluateBrushResponseCurve(curve, 1) == curve.max, "empty curve lost the three-value response");
}

// 편집한 곡선의 역순·중복·비유한 매듭을 단독 평가와 프리셋 해석에서 거부한다.
void invalidCurves()
{
    BrushPreset preset;
    preset.dynamics.sizeResponse.enabled = true;
    auto& curve = preset.dynamics.sizeResponse.pressure;
    curve.enabled = true;
    const std::array invalidInputs{.25, .5, std::numeric_limits<double>::quiet_NaN()};
    for (const auto input : invalidInputs) {
        curve.points = {{0, .1}, {.5, .45}, {input, .7}, {1, 1}};
        checkRejected([&] { (void)evaluateBrushResponseCurve(curve, .05); },
                      "invalid interior knot was accepted before a curve evaluation");
        const auto resolved = resolveBrushPreset(preset);
        check(!resolved.brush && !resolved.errors.empty(), "preset resolution accepted an invalid curve");
    }

    curve.points = {{0, .1}, {.5, .45}, {.75, .7}, {1, 1}};
    const auto payload = serializeBrushPreset(preset);
    const auto valid = readBrushPreset(payload);
    check(valid.preset && valid.preset->dynamics.sizeResponse.pressure.points[2].input == .75,
          "sorted knots did not round-trip");
    const std::string key = "dynamics.sizeResponse.pressure.points.2.input\t";
    const auto field = payload.find(key);
    check(field != std::string::npos, "preset fixture is missing the interior knot");
    const auto value = field + key.size();
    for (const auto malformed : {"0.25", "0.5"}) {
        auto edited = payload;
        edited.replace(value, payload.find('\n', value) - value, malformed);
        const auto loaded = readBrushPreset(edited);
        check(!loaded.preset && !loaded.errors.empty(), "preset loader accepted an unordered or duplicate knot");
    }

    BrushDynamics bindingSettings;
    BrushDynamicsBinding binding;
    binding.curve.points = {{0, .2}, {.6, .5}, {.4, .7}, {1, 1}};
    bindingSettings.bindings.push_back(binding);
    checkRejected([&] { (void)resolveBrushDynamics(bindingSettings, BrushDynamicsInput{.pressure = 1}); },
                  "dynamics resolution accepted an unordered binding curve");
}

// 직접 만든 BrushState를 바꿔도 잘못된 곡선은 dab을 만들기 전에 걸러야 한다.
void editedStroke()
{
    BrushState brush;
    brush.rasterizer.brushSize = 8;
    brush.rasterizer.spacing = 1;
    brush.dynamics.sizeResponse.enabled = true;
    auto& curve = brush.dynamics.sizeResponse.pressure;
    curve.enabled = true;
    curve.points = {{0, .2}, {.5, .6}, {.75, .9}, {1, 1}};
    RasterDabStream stream;
    const auto first = appendRasterDabs(stream, StrokePoint{.pressure = .5}, brush);
    check(first.size() == 1 && first.front().scale == .6 && stream.active, "valid curve did not render a dab");
    const auto previous = stream;
    curve.points[2].input = .2;
    checkRejected([&] { (void)resolveBrushDynamics(brush.dynamics, BrushDynamicsInput{.pressure = .3}); },
                  "direct dynamics resolution accepted an unordered curve");
    const StrokePoint next{.position = {3, 0}, .pressure = .5, .time = 1};
    checkRejected([&] { (void)appendRasterDabs(stream, next, brush); }, "dab stream accepted an unordered curve");
    check(stream.active && stream.sequenceIndex == previous.sequenceIndex && stream.placementIndex == previous.placementIndex
          && stream.traveledDistance == previous.traveledDistance && stream.previousPoint.time == previous.previousPoint.time,
          "invalid curve changed the stroke checkpoint");
    curve.points[2].input = .75;
    check(appendRasterDabs(stream, next, brush, true).size() == 3 && !stream.active,
          "corrected curve did not resume from the same stroke checkpoint");
}
} // namespace

int main()
{
    try {
        exactInterpolation();
        invalidCurves();
        editedStroke();
        std::cout << "All four easing modes have zero bit differences against linear search\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
