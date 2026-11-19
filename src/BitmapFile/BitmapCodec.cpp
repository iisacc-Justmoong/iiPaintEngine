#include "BitmapCodec.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <charconv>
#include <cmath>
#include <cstring>
#include <memory>
#include <span>

#define STBI_NO_STDIO
// ARM64EC는 일부 x64 매크로도 정의하므로 stb의 x86 감지에서 제외한다.
#if defined(_M_ARM64EC) && !defined(STBI_NO_SIMD)
#define STBI_NO_SIMD
#endif
// x86은 stb의 SSE2 선택을 사용한다. ARM은 대상에 NEON이 있는 빌드에서 활성화한다.
#if !defined(STBI_NO_SIMD) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) \
    && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && __has_include(<arm_neon.h>)
#define STBI_NEON
#endif
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// Upstream gates its HDR memory writer on stdio support. Only callback-based
// writers are used here; the application supplies file I/O separately.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace bitmap_codec {
namespace {
using Bytes = std::vector<unsigned char>;
bool isSpace(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}
// Binary Netpbm uses big-endian 16-bit samples and arbitrary Maxval.
// Decode it directly to avoid the upstream reader's native-endian conversion.
std::expected<RasterLayer,std::string> readPnm(std::span<const unsigned char> bytes)
{
    std::size_t cursor = 2;
    const auto number = [&]() -> std::expected<int,std::string> {
        while (cursor < bytes.size()) {
            if (isSpace(bytes[cursor])) { ++cursor; continue; }
            if (bytes[cursor] == '#') {
                while (cursor < bytes.size() && bytes[cursor] != '\n' && bytes[cursor] != '\r') ++cursor;
                continue;
            }
            break;
        }
        const auto start = cursor;
        while (cursor < bytes.size() && bytes[cursor] >= '0' && bytes[cursor] <= '9') ++cursor;
        int value = 0;
        const auto *first = reinterpret_cast<const char *>(bytes.data() + start);
        const auto *last = reinterpret_cast<const char *>(bytes.data() + cursor);
        const auto parsed = std::from_chars(first,last,value);
        if (parsed.ec != std::errc{} || parsed.ptr != last || cursor == start)
            return std::unexpected("Invalid binary PNM header.");
        return value;
    };
    if (bytes.size() <= cursor || !isSpace(bytes[cursor])) return std::unexpected("Invalid PNM signature.");
    const auto width = number(),height = number(),maximum = number();
    if (!width || !height || !maximum || !validDimensions(*width,*height) || *maximum < 1 || *maximum > 65535)
        return std::unexpected("Invalid binary PNM dimensions or maximum sample value.");
    if (cursor == bytes.size() || !isSpace(bytes[cursor])) return std::unexpected("PNM raster delimiter is missing.");
    ++cursor; // Exactly one whitespace byte separates the header and binary raster.
    const int channels = bytes[1] == '6' ? 3 : 1;
    const int sampleBytes = *maximum < 256 ? 1 : 2;
    const auto required = static_cast<std::size_t>(*width) * *height * channels * sampleBytes;
    if (bytes.size() - cursor < required) return std::unexpected("Binary PNM raster is truncated.");
    auto pixels = makeRasterLayer(*width,*height);
    for (auto &argb : pixels.pixels) {
        std::uint32_t values[3]{};
        for (int channel = 0;channel < channels;++channel) {
            std::uint32_t value = bytes[cursor++];
            if (sampleBytes == 2) value = (value << 8) | bytes[cursor++];
            if (value > static_cast<std::uint32_t>(*maximum)) return std::unexpected("PNM sample exceeds its maximum value.");
            values[channel] = (value * 255U + *maximum / 2U) / *maximum;
        }
        if (channels == 1) values[1] = values[2] = values[0];
        argb = 0xFF000000U | values[0] << 16 | values[1] << 8 | values[2];
    }
    return pixels;
}
bool starts(std::span<const unsigned char> bytes, std::string_view signature)
{
    return bytes.size() >= signature.size() && std::equal(signature.begin(),signature.end(),bytes.begin(),
            [](char a,unsigned char b) { return static_cast<unsigned char>(a) == b; });
}
std::string detectFormat(std::span<const unsigned char> b)
{
    if (starts(b,"\x89PNG\r\n\x1a\n")) return "png";
    if (b.size() >= 3 && b[0] == 255 && b[1] == 216 && b[2] == 255) return "jpeg";
    if (starts(b,"BM")) return "bmp";
    if (starts(b,"GIF87a") || starts(b,"GIF89a")) return "gif";
    if (starts(b,"8BPS")) return "psd";
    if (starts(b,"P5") || starts(b,"P6")) return "pnm";
    if (starts(b,"#?RADIANCE") || starts(b,"#?RGBE")) return "hdr";
    if (b.size() >= 92 && b[0] == 0x53 && b[1] == 0x80 && b[2] == 0xF6 && b[3] == 0x34
            && b[88] == 'P' && b[89] == 'I' && b[90] == 'C' && b[91] == 'T') return "pic";
    if (b.size() >= 18 && b[1] <= 1 && (b[2] == 1 || b[2] == 2 || b[2] == 3
            || b[2] == 9 || b[2] == 10 || b[2] == 11)
            && (b[16] == 8 || b[16] == 15 || b[16] == 16 || b[16] == 24 || b[16] == 32)) return "tga";
    return {};
}
struct Output { Bytes bytes; bool failed = false; };
void appendBytes(void *context,void *data,int size)
{
    auto &output = *static_cast<Output *>(context);
    if (output.failed) return;
    if (size < 0 || output.bytes.size() + static_cast<std::size_t>(size) > maxBitmapEncodedBytes) {
        output.failed = true; return;
    }
    try {
        auto begin = static_cast<const unsigned char *>(data);
        output.bytes.insert(output.bytes.end(),begin,begin + size);
    } catch (...) { output.failed = true; }
}

// 리틀엔디언 RGBA 워드와 ARGB32 사이에서 빨강과 파랑을 교환한다.
std::uint32_t swapRedBlue(std::uint32_t word) noexcept
{
    return (word & 0xff00ff00U) | ((word & 0x000000ffU) << 16) | ((word & 0x00ff0000U) >> 16);
}

// 같은 바이트 치환으로 RGBA와 리틀엔디언 ARGB32 저장소를 양방향 변환한다.
void swapRedBluePixels(const unsigned char *source, unsigned char *destination, std::size_t count) noexcept
{
    std::size_t i = 0;
#if defined(STBI_NEON) && defined(__aarch64__)
    constexpr std::array<unsigned char, 16> channels{2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15};
    const auto shuffle = vld1q_u8(channels.data());
    for (; count - i >= 4; i += 4) {
        const auto group = vld1q_u8(source + i * 4);
        vst1q_u8(destination + i * 4, vqtbl1q_u8(group, shuffle));
    }
#endif
    for (; i < count; ++i) {
        std::uint32_t word;
        std::memcpy(&word, source + i * 4, sizeof(word));
        const auto converted = swapRedBlue(word);
        std::memcpy(destination + i * 4, &converted, sizeof(converted));
    }
}

// 디코더의 RGBA 바이트를 최종 ARGB32 벡터에 기록한다.
void unpackRgba(const unsigned char *rgba, std::span<std::uint32_t> pixels) noexcept
{
    if constexpr (std::endian::native == std::endian::little) {
        swapRedBluePixels(rgba, reinterpret_cast<unsigned char *>(pixels.data()), pixels.size());
    } else {
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const auto *color = rgba + i * 4;
            pixels[i] = static_cast<std::uint32_t>(color[3]) << 24 | static_cast<std::uint32_t>(color[0]) << 16
                      | static_cast<std::uint32_t>(color[1]) << 8 | color[2];
        }
    }
}

// 알파 포함 포맷용 연속 RGBA 배열을 기존 ARGB32 픽셀에서 채운다.
void packRgba(std::span<const std::uint32_t> pixels, unsigned char *rgba) noexcept
{
    if constexpr (std::endian::native == std::endian::little) {
        swapRedBluePixels(reinterpret_cast<const unsigned char *>(pixels.data()), rgba, pixels.size());
    } else {
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const auto argb = pixels[i];
            auto *color = rgba + i * 4;
            color[0] = static_cast<unsigned char>(argb >> 16);
            color[1] = static_cast<unsigned char>(argb >> 8);
            color[2] = static_cast<unsigned char>(argb);
            color[3] = static_cast<unsigned char>(argb >> 24);
        }
    }
}

// 기존 8비트 알파 합성 반올림을 적용한 뒤 대상 포맷의 채널을 바로 기록한다.
template <typename Channel, typename Convert>
void packOpaqueRgb(std::span<const std::uint32_t> pixels, std::uint32_t backgroundArgb,
                   Channel *output, Convert convert) noexcept
{
    for (const auto argb : pixels) {
        const auto alpha = argb >> 24;
        for (const unsigned shift : {16U, 8U, 0U}) {
            const auto foreground = (argb >> shift) & 255U;
            const auto background = (backgroundArgb >> shift) & 255U;
            const auto color = static_cast<unsigned char>((foreground * alpha + background * (255 - alpha) + 127) / 255);
            *output++ = convert(color);
        }
    }
}
}

std::array<float, 256> hdrLinearTable()
{
    const auto compute = [] {
        std::array<float, 256> table;
        for (std::size_t i = 0; i < table.size(); ++i) {
            table[i] = static_cast<float>(std::pow(i / 255.0, 2.2));
        }
        return table;
    };
#ifdef FE_TONEAREST
    if (std::fegetround() == FE_TONEAREST) {
        static const auto nearest = compute();
        return nearest;
    }
#endif
    // 방향 반올림 설정에서는 해당 모드에서 기존 pow와 float 변환을 수행한다.
    return compute();
}

std::string normalizeFormat(std::string_view text)
{
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    std::string format(text);
    std::ranges::transform(format,format.begin(),[](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    });
    if (!format.empty() && format[0] == '.') format.erase(0,1);
    if (format == "jpg" || format == "jfif") return "jpeg";
    if (format == "ppm" || format == "pgm") return "pnm";
    return format;
}
bool validDimensions(int width,int height)
{
    return width > 0 && height > 0 && width <= maxBitmapDimension && height <= maxBitmapDimension
            && static_cast<std::uint64_t>(width) * height <= maxBitmapPixelCount;
}
std::expected<DecodedBitmap,std::string> read(std::span<const unsigned char> bytes)
{
    try {
        if (bytes.empty() || bytes.size() > maxBitmapEncodedBytes)
            return std::unexpected("Encoded bitmap size is outside the supported range.");
        const auto format = detectFormat(bytes);
        if (format.empty()) return std::unexpected("The content is not a supported bitmap format.");
        if (format == "pnm") {
            auto pixels = readPnm(bytes);
            if (!pixels) return std::unexpected(pixels.error());
            return DecodedBitmap{std::move(*pixels),format};
        }
        int width = 0,height = 0,channels = 0;
        const auto size = static_cast<int>(bytes.size());
        if (!stbi_info_from_memory(bytes.data(),size,&width,&height,&channels) || !validDimensions(width,height))
            return std::unexpected("Bitmap dimensions or header are invalid or exceed the configured limits.");
        const std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> image{
                stbi_load_from_memory(bytes.data(),size,&width,&height,&channels,4),stbi_image_free};
        if (!image) return std::unexpected("The bitmap decoder returned no pixels.");
        if (!validDimensions(width,height)) return std::unexpected("Decoded dimensions exceed the configured limits.");
        auto pixels = makeRasterLayer(width,height);
        unpackRgba(image.get(), pixels.pixels);
        return DecodedBitmap{std::move(pixels),format};
    } catch (const std::exception &e) { return std::unexpected(std::string(e.what())); }
}
std::expected<BitmapFileIO::Bytes,std::string> write(std::string_view requested,const RasterLayer &pixels,
                                                     const BitmapFileWriteOptions &options)
{
    try {
        const auto format = normalizeFormat(requested);
        if (std::ranges::find(supportedBitmapWriteFormats(),format) == supportedBitmapWriteFormats().end())
            return std::unexpected("Unsupported bitmap writer: " + format);
        if (!validDimensions(pixels.width,pixels.height)
                || pixels.pixels.size() != static_cast<std::size_t>(pixels.width) * pixels.height)
            return std::unexpected("Invalid bitmap pixels.");
        if (options.quality < -1 || options.quality > 100) return std::unexpected("Quality must be -1 or within [0,100].");
        Output output;
        int ok = 0;
        const auto pixelCount = pixels.pixels.size();
        const auto unchangedChannel = [](unsigned char channel) noexcept { return channel; };
        if (format == "hdr") {
            const auto table = hdrLinearTable();
            const auto linear = std::make_unique_for_overwrite<float[]>(pixelCount * 3);
            packOpaqueRgb(pixels.pixels, options.backgroundArgb, linear.get(),
                          [&table](unsigned char channel) noexcept { return table[channel]; });
            ok = stbi_write_hdr_to_func(appendBytes, &output, pixels.width, pixels.height, 3, linear.get());
        } else if (format == "pnm") {
            const auto header = "P6\n" + std::to_string(pixels.width) + " " + std::to_string(pixels.height) + "\n255\n";
            output.bytes.resize(header.size() + pixelCount * 3);
            std::copy(header.begin(), header.end(), output.bytes.begin());
            packOpaqueRgb(pixels.pixels, options.backgroundArgb, output.bytes.data() + header.size(), unchangedChannel);
            ok = 1;
        } else if (format == "jpeg") {
            const auto rgb = std::make_unique_for_overwrite<unsigned char[]>(pixelCount * 3);
            packOpaqueRgb(pixels.pixels, options.backgroundArgb, rgb.get(), unchangedChannel);
            ok = stbi_write_jpg_to_func(appendBytes, &output, pixels.width, pixels.height, 3,
                                       rgb.get(), options.quality < 0 ? 90 : options.quality);
        } else {
            const auto rgba = std::make_unique_for_overwrite<unsigned char[]>(pixelCount * 4);
            packRgba(pixels.pixels, rgba.get());
            if (format == "png") ok = stbi_write_png_to_func(appendBytes, &output, pixels.width, pixels.height, 4,
                                                             rgba.get(), pixels.width * 4);
            else if (format == "bmp") ok = stbi_write_bmp_to_func(appendBytes, &output, pixels.width, pixels.height, 4, rgba.get());
            else if (format == "tga") ok = stbi_write_tga_to_func(appendBytes, &output, pixels.width, pixels.height, 4, rgba.get());
        }
        if (!ok || output.failed || output.bytes.empty()) return std::unexpected("Bitmap encoding failed.");
        return std::move(output.bytes);
    } catch (const std::exception &e) { return std::unexpected(std::string(e.what())); }
}
}
