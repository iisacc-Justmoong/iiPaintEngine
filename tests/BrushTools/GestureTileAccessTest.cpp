#include <iiBrushTools/Gesture.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

using namespace ii::brush;

namespace {
std::size_t tileLookups = 0;
constexpr DevicePixelRect sourceBounds{{-22, -18}, 215, 115};
constexpr DevicePixelRect singleTile{{-28, -24}, 28, 24};
constexpr DevicePixelRect manyTiles{{-28, -24}, 248, 150};

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::uint32_t color(DevicePixelPoint local)
{
    return 0xff000000U | (static_cast<std::uint32_t>(local.x) << 16)
           | (static_cast<std::uint32_t>(local.y) << 8) | 0x46U;
}

// 서로 다른 원본·클립 영역에서 두 콜백이 같은 연속 ARGB 저장소를 읽는다.
struct SourceProbe {
    SourceProbe()
    {
        pixels.reserve(static_cast<std::size_t>(sourceBounds.width) * sourceBounds.height);
        for (int y = 0; y < sourceBounds.height; ++y) {
            for (int x = 0; x < sourceBounds.width; ++x) pixels.push_back(color({x, y}));
        }
    }

    std::vector<std::uint32_t> pixels;
    mutable std::size_t pixelCalls = 0, rowCalls = 0, copiedPixels = 0;
};

// 원본 범위 밖으로 콜백을 호출했는지 확인하면서 ARGB를 반환한다.
std::uint32_t samplePixel(const void* context, DevicePixelPoint local)
{
    check(local.x >= 0 && local.x < sourceBounds.width && local.y >= 0 && local.y < sourceBounds.height,
          "source pixel was read outside its bounds");
    const auto& source = *static_cast<const SourceProbe*>(context);
    ++source.pixelCalls;
    return source.pixels[static_cast<std::size_t>(local.y) * sourceBounds.width + local.x];
}

// 인접 픽셀을 한 콜백에서 읽고, 잘린 행의 시작점과 폭을 함께 검사한다.
void copyRow(const void* context, DevicePixelPoint start, std::span<std::uint32_t> destination)
{
    check(start.x >= 0 && start.x < sourceBounds.width && start.y >= 0 && start.y < sourceBounds.height
          && !destination.empty() && destination.size() <= static_cast<std::size_t>(sourceBounds.width - start.x),
          "source row was read outside its bounds");
    const auto& source = *static_cast<const SourceProbe*>(context);
    ++source.rowCalls;
    source.copiedPixels += destination.size();
    const auto offset = static_cast<std::size_t>(start.y) * sourceBounds.width + start.x;
    std::copy_n(source.pixels.data() + offset, destination.size(), destination.data());
}

// 픽셀 외에도 결과에 공개되는 행·열 순서와 클립된 타일 경계를 고정한다.
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

// 오른쪽 아래에서 시작해 음수 좌표로 이동한다. 타일 생성 순서와 반환 순서가 다르다.
void renderAndCheck(DevicePixelRect clip, std::uint64_t golden, std::size_t maxLookups)
{
    Controls controls;
    for (const auto parameter : {Parameter::Stabilization, Parameter::PostCorrection, Parameter::Pressure,
                                 Parameter::Tilt, Parameter::Velocity, Parameter::Scatter, Parameter::Start,
                                 Parameter::End}) {
        check(controls.set(parameter, 0), "control setup failed");
    }
    check(controls.set(Parameter::Size, 12) && controls.set(Parameter::Spacing, 50), "brush setup failed");
    const std::array points{
        StrokePoint{.position = {198, 93}, .time = 0},
        StrokePoint{.position = {-16, 93}, .time = 1},
        StrokePoint{.position = {-16, -12}, .time = 2},
        StrokePoint{.position = {-8, -12}, .time = 3},
    };
    SourceProbe source;
    const RenderTarget target{clip, {&source, samplePixel, sourceBounds.origin, sourceBounds.width,
                                     sourceBounds.height}, 0xb0d83971U};
    tileLookups = 0;
    const auto stroke = renderGesture(points, Tool::Paint, controls, target);
    if (!stroke) throw std::runtime_error(stroke.error().message);
    const auto hash = pixelHash(*stroke);
    const auto scalarCalls = source.pixelCalls;
    const auto scalarLookups = tileLookups;
    check(hash == golden, "tile order or ARGB golden changed");
    check(tileLookups > 0 && tileLookups <= maxLookups, "too many tile index lookups");

    source.pixelCalls = 0;
    tileLookups = 0;
    const auto bulk = renderGesture(points, Tool::Paint, controls, target, {}, copyRow);
    if (!bulk) throw std::runtime_error(bulk.error().message);
    std::cout << "tiles=" << stroke->tiles.size() << " lookups=" << tileLookups
              << " pixel_reads=" << scalarCalls << " row_reads=" << source.rowCalls
              << " hash=0x" << std::hex << hash << std::dec << '\n';
    check(pixelHash(*bulk) == golden && bulk->length == stroke->length && bulk->dabCount == stroke->dabCount,
          "row copy changed the stroke");
    check(tileLookups == scalarLookups && source.copiedPixels == scalarCalls && source.pixelCalls == 0
          && source.rowCalls > 0 && source.rowCalls < scalarCalls / 8,
          "row copy did not replace the source pixel reads");
}
} // namespace

namespace ii::brush {
// Gesture.cpp의 테스트 빌드에서 실제 인덱스를 탐색할 때 호출한다.
void countGestureTileLookup() noexcept { ++tileLookups; }
} // namespace ii::brush

int main()
{
    try {
        renderAndCheck(singleTile, 0xed40fd3072c6fba4ULL, 1);
        renderAndCheck(manyTiles, 0xecb6cc2c80992a45ULL, 400);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
