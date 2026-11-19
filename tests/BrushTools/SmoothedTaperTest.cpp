// Gesture.cpp는 테스트 전용으로 호출 이름을 바꿔 빌드한다. 여기서는 실제 생성 함수를 호출한다.
#undef appendRasterDabs
#include <iiBrushTools/Gesture.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace ii::brush;

namespace {
std::size_t appendCalls = 0;
std::size_t finishCalls = 0;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// 타일 순서, 경계와 ARGB 값을 정수 단위로 비교하므로 메모리의 바이트 순서와 무관하다.
std::uint64_t pixelHash(const RenderedStroke& stroke)
{
    std::uint64_t hash = 14695981039346656037ULL;
    const auto add = [&hash](std::uint32_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    for (const auto& tile : stroke.tiles) {
        add(tile.bounds.origin.x);
        add(tile.bounds.origin.y);
        add(tile.bounds.width);
        add(tile.bounds.height);
        for (const auto pixel : tile.pixels.pixels) add(pixel);
    }
    return hash;
}
} // namespace

// 테스트 빌드의 모든 호출을 세면서 실제 스트림 처리와 dab 생성을 그대로 수행한다.
std::vector<BrushDab> countedAppendRasterDabs(RasterDabStream& stream, const StrokePoint& point,
                                            const BrushState& brush, bool finish)
{
    ++appendCalls;
    finishCalls += finish;
    return appendRasterDabs(stream, point, brush, finish);
}

int main()
{
    try {
        Controls controls;
        for (const auto parameter : {Parameter::Stabilization, Parameter::PostCorrection, Parameter::Pressure,
                                     Parameter::Tilt, Parameter::Velocity, Parameter::Scatter}) {
            check(controls.set(parameter, 0), "control setup failed");
        }
        check(controls.set(Parameter::Size, 16) && controls.set(Parameter::Spacing, 50), "dab setup failed");

        auto brush = resolve(Tool::Paint, controls, 0xff507dc3U);
        check(brush.has_value(), "brush setup failed");
        brush->taper = {.start = .25, .end = .5, .minimumSize = .25, .strength = 1};
        auto& rasterizer = brush->state.rasterizer;
        rasterizer.smoothingEnabled = true;
        rasterizer.smoothingMode = BrushSmoothingMode::PulledString;
        rasterizer.smoothing = .5;
        rasterizer.smoothingRadius = 16;
        rasterizer.smoothingCatchUpOnEnd = true;

        // 정지 이벤트와 세 번의 직각 회전, 끝점 따라잡기가 있는 획을 여러 타일에 그린다.
        const std::array points{
            StrokePoint{.position = {12, 44}, .time = 0},
            StrokePoint{.position = {12, 44}, .time = .125},
            StrokePoint{.position = {44, 44}, .time = .25},
            StrokePoint{.position = {36, 76}, .time = .5},
            StrokePoint{.position = {68, 68}, .time = .75},
            StrokePoint{.position = {60, 38}, .time = 1},
        };
        RenderTarget target{.clip = {{0, 0}, 96, 96}};
        Limits limits;
        limits.maxDabs = 14;
        const auto stroke = renderGestureWithResolvedBrush(points, *brush, controls, target, limits);
        if (!stroke) throw std::runtime_error(stroke.error().message);
        check(stroke->length == 102 && stroke->dabCount == limits.maxDabs, "smoothed length or dab count changed");
        // 중복 제거 전의 렌더에서 얻은 전체 타일 골든이다.
        constexpr std::uint64_t golden = 0xceb67b663eef1ecfULL;
        const auto hash = pixelHash(*stroke);
        if (hash != golden) {
            std::cerr << "ARGB golden mismatch, got 0x" << std::hex << hash << '\n';
            return 1;
        }
        if (appendCalls != points.size() || finishCalls != 1) {
            std::cerr << "expected " << points.size() << " append calls and one finish, got " << appendCalls
                      << " append calls and " << finishCalls << " finishes\n";
            return 1;
        }

        // 전체 dab 예산에 모자라면 원본 샘플링과 타일 합성 전에 실패해야 한다.
        std::size_t sourceReads = 0;
        target.source = {&sourceReads, [](const void* context, DevicePixelPoint) {
            ++*const_cast<std::size_t*>(static_cast<const std::size_t*>(context));
            return std::uint32_t{0};
        }, target.clip.origin, target.clip.width, target.clip.height};
        --limits.maxDabs;
        const auto rejected = renderGestureWithResolvedBrush(points, *brush, controls, target, limits);
        check(!rejected && rejected.error().code == ErrorCode::BudgetExceeded && sourceReads == 0,
              "dab budget must be checked before sampling");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
