// 테스트용 MaskPaintSession.cpp가 이름을 바꾼 병합 함수는 실제 구현을 그대로 호출한다.
#undef uniteDevicePixelRects
#include "Application/MaskPaintSession.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>

struct MaskPaintSessionTestAccess {
    // 포인터 입력에서 고정하는 획 상태를 설정해 회전된 타원과 서로 다른 cap을 직접 검증한다.
    static void begin(MaskPaintSession& session, const BrushState& brush, const LocalMaskBrushOptions& options,
                      std::uint32_t reference)
    {
        session.m_activeBrush = brush;
        session.m_activeOptions = options;
        session.m_referenceArgb = reference;
    }
    static DevicePixelRect apply(MaskPaintSession& session, std::span<const BrushDab> dabs) { return session.applyDabs(dabs); }
    static const PixelMask& pending(const MaskPaintSession& session) { return session.m_pending; }
    static DevicePixelRect pendingDirty(const MaskPaintSession& session) { return session.m_pendingDirty; }
    static DevicePixelRect previewDirty(const MaskPaintSession& session) { return session.m_dirty; }
};

namespace {
std::size_t dirtyUnions = 0;
std::size_t totalUnions = 0, totalRowBudget = 0;
std::uint32_t maximumUlp = 0;
bool exceededRowBudget = false;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

double unit(double value) { return std::clamp(value, 0., 1.); }

bool sameBounds(DevicePixelRect a, DevicePixelRect b)
{
    return a.origin.x == b.origin.x && a.origin.y == b.origin.y && a.width == b.width && a.height == b.height;
}

// 원본의 Auto Mask 색상 거리·허용치·알파 모델을 기준값으로 계산한다.
double referenceWeight(std::uint32_t candidate, std::uint32_t reference, double tolerance)
{
    const auto alpha = static_cast<double>(candidate >> 24) / 255;
    if (alpha == 0) return 0;
    double squareDistance = 0;
    for (const auto shift : {16U, 8U, 0U}) {
        const auto delta = (static_cast<double>((candidate >> shift) & 255U)
                         - static_cast<double>((reference >> shift) & 255U)) / 255;
        squareDistance += delta * delta;
    }
    const auto distance = std::sqrt(squareDistance / 3);
    if (tolerance == 0) return distance == 0 ? alpha : 0;
    const auto t = unit(distance / tolerance);
    return alpha * (1 - t * t * (3 - 2 * t));
}

struct ReferenceMask {
    PixelMask preview, pending;
    DevicePixelRect pendingDirty{}, previewDirty{};
};

// 변경 전 구현과 같은 hypot과 픽셀 단위 사각형 병합으로 각 이벤트의 골든을 계산한다.
DevicePixelRect referenceApply(ReferenceMask& state, const PixelMask& committed,
    std::span<const BrushDab> dabs, const Rasterizer& rasterizer, const LocalMaskBrushOptions& options,
    std::span<const std::uint32_t> guidance, std::uint32_t reference)
{
    DevicePixelRect changed{};
    for (const auto& dab : dabs) {
        const auto bounds = clippedDeviceBoundsForBrushDab(dab, rasterizer, {{}, committed.width, committed.height});
        const auto rx = std::max(.01, rasterizer.radius * dab.scale * dab.ellipseScaleX);
        const auto ry = std::max(.01, rasterizer.radius * dab.scale * dab.ellipseScaleY);
        const auto cosine = std::cos(dab.rotationRadians), sine = std::sin(dab.rotationRadians);
        for (int y = bounds.origin.y; y < bounds.origin.y + bounds.height; ++y) {
            for (int x = bounds.origin.x; x < bounds.origin.x + bounds.width; ++x) {
                const auto dx = x - dab.position.x, dy = y - dab.position.y;
                const auto distance = std::hypot((dx * cosine + dy * sine) / rx, (-dx * sine + dy * cosine) / ry);
                const auto edge = unit((1 - distance) * std::min(rx, ry) + .5);
                const auto falloff = options.feather == 0 ? 1 : unit((1 - distance) / options.feather);
                const auto index = static_cast<std::size_t>(y) * committed.width + x;
                const auto weight = options.autoMask ? referenceWeight(guidance[index], reference, options.autoMaskTolerance) : 1.;
                const auto alpha = unit(options.flow * edge * falloff * weight);
                if (alpha == 0) continue;
                const auto pending = state.pending.coverage[index];
                const auto cap = unit(dab.opacityCapScale);
                const auto accumulated = pending >= cap ? pending : std::min(cap, unit(pending + (1 - pending) * alpha));
                const auto next = static_cast<float>(accumulated);
                if (next != pending) {
                    state.pending.coverage[index] = next;
                    if (pending == 0.F) state.pendingDirty = uniteDevicePixelRects(state.pendingDirty, {{x, y}, 1, 1});
                }
                const auto effect = unit(options.strokeOpacity * accumulated);
                const auto base = committed.coverage[index];
                const auto result = options.mode == MaskBrushMode::Erase ? base * (1 - effect)
                    : base + std::max(0., options.density - base) * effect;
                const auto coverage = static_cast<float>(unit(result));
                if (coverage != state.preview.coverage[index]) {
                    state.preview.coverage[index] = coverage;
                    changed = uniteDevicePixelRects(changed, {{x, y}, 1, 1});
                }
            }
        }
    }
    state.previewDirty = uniteDevicePixelRects(state.previewDirty, changed);
    return changed;
}

// 정규화된 양의 float에서 인접 비트 패턴은 1 ULP 차이다. 두 부호의 0도 허용한다.
void checkCoverage(const PixelMask& actual, const PixelMask& golden)
{
    check(actual.width == golden.width && actual.height == golden.height, "mask dimensions changed");
    for (std::size_t i = 0; i < golden.coverage.size(); ++i) {
        const auto value = actual.coverage[i], expected = golden.coverage[i];
        if (value == expected) continue;
        check(value >= 0 && value <= 1, "coverage is invalid");
        const auto a = std::bit_cast<std::uint32_t>(value), b = std::bit_cast<std::uint32_t>(expected);
        const auto ulp = std::max(a, b) - std::min(a, b);
        maximumUlp = std::max(maximumUlp, ulp);
        if (ulp > 1) {
            std::cerr << "pixel=" << i << " actual=" << value << " expected=" << expected << " ulp=" << ulp << '\n';
            throw std::runtime_error("mask coverage differs from the scalar golden by more than 1 ULP");
        }
    }
}

// 화면 밖 dab, 가로로 잘린 회전 타원, 겹침, 매우 작은 반경, 투명 가이드를 섞는다.
void verify(LocalMaskBrushOptions options)
{
    auto committed = makePixelMask(69, 47);
    constexpr std::uint32_t reference = 0xff605070U;
    std::vector<std::uint32_t> guidance(committed.coverage.size());
    for (std::size_t i = 0; i < guidance.size(); ++i) {
        committed.coverage[i] = static_cast<float>((i * 5) % 9) / 8.F;
        switch (i % 6) {
            case 0: guidance[i] = 0x00605070U; break;
            case 1: guidance[i] = 0x80605070U; break;
            case 2: guidance[i] = 0xff282852U; break;
            case 3: guidance[i] = 0xff0a0af0U; break;
            default: guidance[i] = reference; break;
        }
    }
    BrushState brush;
    brush.rasterizer.radius = 21;
    brush.rasterizer.brushSize = 42;
    brush.rasterizer.proceduralTip = true;
    const std::array dabs{
        BrushDab{.position = {1.375, 14.0625}, .rotationRadians = .42, .opacityCapScale = .58, .ellipseScaleY = .76},
        BrushDab{.position = {25.725, 18.375}, .rotationRadians = -.81, .opacityCapScale = .9,
                 .ellipseScaleX = .82, .ellipseScaleY = 1.07},
        BrushDab{.position = {53.5, 34.625}, .scale = .87, .rotationRadians = .23, .opacityCapScale = .75},
        BrushDab{.position = {8, 21}, .scale = .001, .opacityCapScale = .13},
        BrushDab{.position = {24, 8}, .scale = .75, .rotationRadians = -.5, .opacityCapScale = .12},
        BrushDab{.position = {-500, -500}},
    };

    MaskPaintSession session;
    check(session.replaceMask(committed) && (!options.autoMask || session.setColorGuidance(guidance)), "mask fixture setup");
    MaskPaintSessionTestAccess::begin(session, brush, options, reference);
    ReferenceMask golden{committed, makePixelMask(committed.width, committed.height)};
    const std::span<const BrushDab> stroke = dabs;
    for (const auto batch : {stroke.first(2), stroke.subspan(2)}) {
        const auto expected = referenceApply(golden, committed, batch, brush.rasterizer, options, guidance, reference);
        dirtyUnions = 0;
        const auto changed = MaskPaintSessionTestAccess::apply(session, batch);
        checkCoverage(session.previewMask(), golden.preview);
        checkCoverage(MaskPaintSessionTestAccess::pending(session), golden.pending);
        check(sameBounds(changed, expected) && sameBounds(MaskPaintSessionTestAccess::pendingDirty(session), golden.pendingDirty)
              && sameBounds(MaskPaintSessionTestAccess::previewDirty(session), golden.previewDirty), "dirty rectangle changed");

        std::size_t rowBudget = 1; // 마지막 획 전체 dirty 합집합과 행마다 두 평면을 최대 한 번씩 합친다.
        for (const auto& dab : batch) {
            rowBudget += 2U * clippedDeviceBoundsForBrushDab(dab, brush.rasterizer, {{}, committed.width, committed.height}).height;
        }
        totalUnions += dirtyUnions;
        totalRowBudget += rowBudget;
        exceededRowBudget |= dirtyUnions > rowBudget;
    }
    session.cancelStroke();
    check(session.previewMask().coverage == committed.coverage
          && std::ranges::all_of(MaskPaintSessionTestAccess::pending(session).coverage, [](auto value) { return value == 0.F; }),
          "cancel failed to clear both planes");
}

// 원이 마스크 모서리에 접할 때 float 반올림과 변경 사각형을 기준 경로와 비교한다.
void verifyEdgeContact()
{
    const auto committed = makePixelMask(48, 3);
    const std::array contacts{std::array{8, 15, 17}, std::array{15, 8, 17}, std::array{7, 24, 25},
                              std::array{24, 7, 25}, std::array{9, 40, 41}, std::array{40, 9, 41}};
    for (const auto [x, y, radius] : contacts) {
        for (const auto feather : {0., .5}) {
            const LocalMaskBrushOptions options{.sizePixels = 2. * radius, .feather = feather};
            BrushState brush;
            brush.rasterizer.radius = radius;
            brush.rasterizer.brushSize = options.sizePixels;
            brush.rasterizer.proceduralTip = true;
            const auto scale = feather == 0 ? (radius - .5) / radius : 1.;
            const std::array dabs{BrushDab{.position = {-static_cast<double>(x), -static_cast<double>(y)}, .scale = scale}};
            MaskPaintSession session;
            check(session.replaceMask(committed), "edge contact fixture setup");
            MaskPaintSessionTestAccess::begin(session, brush, options, 0);
            ReferenceMask golden{committed, makePixelMask(committed.width, committed.height)};
            const auto expected = referenceApply(golden, committed, dabs, brush.rasterizer, options, {}, 0);
            const auto changed = MaskPaintSessionTestAccess::apply(session, dabs);
            checkCoverage(session.previewMask(), golden.preview);
            checkCoverage(MaskPaintSessionTestAccess::pending(session), golden.pending);
            check(sameBounds(changed, expected)
                  && sameBounds(MaskPaintSessionTestAccess::pendingDirty(session), golden.pendingDirty), "edge contact changed dirty bounds");
        }
    }
}
} // namespace

DevicePixelRect countedUniteDevicePixelRects(DevicePixelRect left, DevicePixelRect right)
{
    ++dirtyUnions;
    return uniteDevicePixelRects(left, right);
}

int main()
{
    try {
        LocalMaskBrushOptions options{.sizePixels = 42, .feather = .63, .flow = .36, .density = .73, .strokeOpacity = .61};
        verify(options);
        options.feather = 0;
        options.mode = MaskBrushMode::Erase;
        verify(options);
        options.autoMask = true;
        options.autoMaskTolerance = 0;
        verify(options);
        options.feather = .28;
        options.autoMaskTolerance = .35;
        options.mode = MaskBrushMode::Add;
        verify(options);
        verifyEdgeContact();
        std::cout << "max_coverage_ulp=" << maximumUlp << " dirty_unions=" << totalUnions
                  << " row_budget=" << totalRowBudget << '\n';
        check(!exceededRowBudget, "dirty rectangles were merged more often than the row budget");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
