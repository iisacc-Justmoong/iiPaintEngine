#include "BitmapFile/BitmapCodec.hpp"

#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>

// 기존 코덱의 스칼라 JPEG 및 HDR 경로를 독립 기준으로 빌드한다.
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
// HDR 저장 동안 생성하는 3바이트/픽셀 중간 배열을 계측한다.
struct RgbAllocationProbe;
thread_local RgbAllocationProbe* activeProbe = nullptr;
struct RgbAllocationProbe {
    explicit RgbAllocationProbe(std::size_t size) : byteCount(size) { activeProbe = this; }
    ~RgbAllocationProbe() { activeProbe = nullptr; }
    std::size_t byteCount;
    std::size_t allocations = 0;
};
} // namespace

void* operator new(std::size_t size)
{
    if (auto* pointer = std::malloc(std::max<std::size_t>(size, 1))) {
        if (activeProbe && size == activeProbe->byteCount) ++activeProbe->allocations;
        return pointer;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

namespace {
using Bytes = BitmapFileIO::Bytes;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// 불투명 행은 256개 채널 값을 모두 지난다. 나머지 행은 배경과 알파 평탄화를 검증한다.
RasterLayer colorFixture(int width, int height)
{
    auto image = makeRasterLayer(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto alpha = y == 0 ? 255U : x == 1 && y == 1 ? 0U : static_cast<unsigned>((x * 3 + y * 17) & 255);
            const auto r = static_cast<unsigned>((x * 197 + y * 23) & 255);
            const auto g = static_cast<unsigned>((x * 29 + y * 83) & 255);
            const auto b = static_cast<unsigned>((x * 17 + y * 151) & 255);
            image.pixels[static_cast<std::size_t>(y) * width + x] = alpha << 24 | r << 16 | g << 8 | b;
        }
    }
    return image;
}

// 변경 전 8비트 RGB 중간 버퍼의 정수 반올림을 보존한 기준 구현이다.
Bytes referenceRgb(const RasterLayer& image, std::uint32_t background)
{
    Bytes result;
    result.reserve(image.pixels.size() * 3);
    for (const auto pixel : image.pixels) {
        const auto alpha = pixel >> 24;
        for (const auto shift : {16, 8, 0}) {
            const auto foreground = (pixel >> shift) & 255U;
            const auto base = (background >> shift) & 255U;
            result.push_back(static_cast<unsigned char>((foreground * alpha + base * (255 - alpha) + 127) / 255));
        }
    }
    return result;
}

void collect(void* context, void* data, int size)
{
    auto& bytes = *static_cast<Bytes*>(context);
    const auto* begin = static_cast<const unsigned char*>(data);
    bytes.insert(bytes.end(), begin, begin + size);
}

// 같은 인코딩 입력을 pinned stb의 스칼라 디코더로 읽어 모든 ARGB 워드를 비교한다.
void compareScalarDecode(const Bytes& encoded, std::string_view format)
{
    int width = 0, height = 0, channels = 0;
    const std::unique_ptr<unsigned char, decltype(&stbi_image_free)> rgba{
        stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &channels, 4), stbi_image_free};
    const auto actual = bitmap_codec::read(encoded);
    check(rgba && actual && actual->format == format && actual->pixels.width == width && actual->pixels.height == height,
          "optimized decoding rejected the scalar fixture");
    for (std::size_t i = 0; i < actual->pixels.pixels.size(); ++i) {
        const auto* color = rgba.get() + i * 4;
        const auto expected = static_cast<std::uint32_t>(color[3]) << 24
            | static_cast<std::uint32_t>(color[0]) << 16 | static_cast<std::uint32_t>(color[1]) << 8 | color[2];
        check(actual->pixels.pixels[i] == expected, "optimized decode differs from scalar ARGB");
    }
}

// 작은 버퍼와 홀수 폭을 사용해 벡터 끝부분 및 반투명 픽셀을 검사한다.
void losslessRoundTrips()
{
    for (const auto width : {1, 3, 13, 65}) {
        const auto input = colorFixture(width, 5);
        for (const auto format : {"png", "bmp", "tga"}) {
            const auto encoded = bitmap_codec::write(format, input, {});
            check(encoded.has_value(), "lossless fixture encoding failed");
            const auto decoded = bitmap_codec::read(*encoded);
            check(decoded && decoded->pixels.width == input.width && decoded->pixels.height == input.height
                  && decoded->pixels.pixels == input.pixels, "lossless round trip changed ARGB pixels");
            compareScalarDecode(*encoded, format);
        }
    }
}

// 4:2:0 색차 리샘플링과 4:4:4 모두 스칼라 기준과 일치해야 한다.
void jpegParity()
{
    const auto input = colorFixture(41, 23);
    BitmapFileWriteOptions options{.backgroundArgb = 0x173189b2U};
    const auto rgb = referenceRgb(input, options.backgroundArgb);
    for (const auto quality : {80, 97}) {
        options.quality = quality;
        Bytes expected;
        check(stbi_write_jpg_to_func(collect, &expected, input.width, input.height, 3, rgb.data(), quality) != 0,
              "scalar JPEG fixture encoding failed");
        const auto encoded = bitmap_codec::write("jpeg", input, options);
        check(encoded && *encoded == expected, "JPEG encoding changed bytes");
        compareScalarDecode(*encoded, "jpeg");
    }
}

// PNM은 헤더 뒤의 최종 출력 버퍼에 평탄화 RGB를 직접 기록한다.
void pnmParity()
{
    const auto input = colorFixture(19, 7);
    const BitmapFileWriteOptions options{.backgroundArgb = 0x173189b2U};
    const std::string_view header = "P6\n19 7\n255\n";
    const auto rgb = referenceRgb(input, options.backgroundArgb);
    Bytes expected(header.begin(), header.end());
    expected.insert(expected.end(), rgb.begin(), rgb.end());
    const auto encoded = bitmap_codec::write("pnm", input, options);
    check(encoded && *encoded == expected, "PNM output bytes changed");
    const auto decoded = bitmap_codec::read(*encoded);
    check(decoded && decoded->pixels.width == input.width && decoded->pixels.height == input.height, "PNM fixture decode failed");
    for (std::size_t i = 0; i < input.pixels.size(); ++i) {
        const auto argb = 0xff000000U | static_cast<std::uint32_t>(rgb[i * 3]) << 16
            | static_cast<std::uint32_t>(rgb[i * 3 + 1]) << 8 | rgb[i * 3 + 2];
        check(decoded->pixels.pixels[i] == argb, "PNM round trip changed a flattened pixel");
    }
}

// RGBE 양자화에 가려질 수 있는 차이도 검사하도록 LUT의 float 자체를 비교한다.
void checkHdrTableBits()
{
    const auto compare = [] {
        const auto table = bitmap_codec::hdrLinearTable();
        for (std::size_t value = 0; value < table.size(); ++value) {
            const auto expected = static_cast<float>(std::pow(value / 255.0, 2.2));
            check(std::bit_cast<std::uint32_t>(table[value]) == std::bit_cast<std::uint32_t>(expected),
                  "HDR lookup table differs from the original float gamma conversion");
        }
    };
    compare();
    compare();
#ifdef FE_DOWNWARD
    const int initial = std::fegetround();
    if (initial >= 0 && initial != FE_DOWNWARD && std::fesetround(FE_DOWNWARD) == 0) {
        struct RestoreRounding {
            int mode;
            ~RestoreRounding() { std::fesetround(mode); }
        } restore{initial};
        compare();
    }
#endif
    compare();
}

// 기존 pow 채널 변환을 RGBE 바이트와 디코딩 결과까지 비교하고 RGB 중간 할당을 검출한다.
void hdrParity()
{
    const auto input = colorFixture(259, 13);
    const BitmapFileWriteOptions options{.backgroundArgb = 0x173189b2U};
    const auto rgb = referenceRgb(input, options.backgroundArgb);
    std::vector<float> legacy(rgb.size());
    std::ranges::transform(rgb, legacy.begin(), [](unsigned char c) { return static_cast<float>(std::pow(c / 255.0, 2.2)); });
    Bytes expected;
    check(stbi_write_hdr_to_func(collect, &expected, input.width, input.height, 3, legacy.data()) != 0,
          "scalar HDR fixture encoding failed");

    std::size_t stagingAllocations = 0;
    const auto encoded = [&] {
        RgbAllocationProbe probe(rgb.size());
        auto result = bitmap_codec::write("hdr", input, options);
        stagingAllocations = probe.allocations;
        return result;
    }();
    check(encoded && *encoded == expected, "HDR encoding differs from the per-channel pow reference");
    compareScalarDecode(*encoded, "hdr");
    std::cout << "HDR intermediate RGB allocations=" << stagingAllocations << '\n';
    check(stagingAllocations == 0, "HDR created an intermediate RGB byte buffer");
}
} // namespace

int main()
{
    try {
        losslessRoundTrips();
        jpegParity();
        pnmParity();
        checkHdrTableBits();
        hdrParity();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
