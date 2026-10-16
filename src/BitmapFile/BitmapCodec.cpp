#include <iiFileProviderStorage.h>
#include "BitmapCodec.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <charconv>
#include <cmath>
#include <fstream>
#include <memory>
#include <span>

#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// Upstream gates its HDR memory writer on stdio support. Only callback-based
// writers are used here; filesystem paths are handled by C++ streams above.
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
std::expected<Bytes,std::string> readBytes(const std::filesystem::path &path)
{
    std::ifstream input(path,std::ios::binary | std::ios::ate);
    if (!input) return std::unexpected("Cannot open the bitmap file.");
    const auto size = input.tellg();
    if (size <= 0 || static_cast<std::uint64_t>(size) > maxBitmapEncodedBytes)
        return std::unexpected("Encoded bitmap size is outside the supported range.");
    Bytes bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        return std::unexpected("Cannot read all bitmap bytes.");
    return bytes;
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
std::filesystem::path uniqueSibling(const std::filesystem::path &path,std::string_view role)
{
    static std::atomic<std::uint64_t> sequence{};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto result = path;
    result += ".iipaint-" + std::string(role) + "-" + std::to_string(stamp) + "-"
            + std::to_string(sequence.fetch_add(1,std::memory_order_relaxed));
    return result;
}
std::expected<void,std::string> writeBytes(const std::filesystem::path &path,const Bytes &bytes)
{
    const auto temporary = uniqueSibling(path,"write");
    struct Cleanup {
        std::filesystem::path path;
        bool owned = false;
        ~Cleanup() { if (owned) { std::error_code ec; std::filesystem::remove(path,ec); } }
    } cleanup{temporary,false};
    {
        iiFileProvider::native::OutputFile output(temporary,std::ios::binary | std::ios::out | std::ios::noreplace);
        if (!output) return std::unexpected("Cannot create bitmap output beside the destination.");
        cleanup.owned = true;
        output.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) return std::unexpected("Cannot write all bitmap output bytes.");
        output.close();
        if (!output) return std::unexpected("Cannot close bitmap output.");
    }
    std::error_code error;
    iiFileProvider::native::renameFile(temporary,path,error);
    if (!error) return {};
    // Some standard-library implementations cannot replace an existing file.
    // Retain the old bytes until replacement succeeds and restore on failure.
    if (!std::filesystem::is_regular_file(path)) return std::unexpected("Cannot replace the bitmap destination: " + error.message());
    const auto backup = uniqueSibling(path,"backup");
    iiFileProvider::native::renameFile(path,backup,error);
    if (error) return std::unexpected("Cannot preserve the previous bitmap: " + error.message());
    iiFileProvider::native::renameFile(temporary,path,error);
    if (error) {
        std::error_code restoreError;
        iiFileProvider::native::renameFile(backup,path,restoreError);
        if (restoreError) return std::unexpected("Bitmap replacement failed; previous bytes remain at " + backup.string());
        return std::unexpected("Cannot replace the bitmap: " + error.message());
    }
    std::error_code ignored; std::filesystem::remove(backup,ignored);
    return {};
}
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
std::expected<DecodedBitmap,std::string> read(const std::filesystem::path &path)
{
    try {
        auto bytes = readBytes(path);
        if (!bytes) return std::unexpected(bytes.error());
        const auto format = detectFormat(*bytes);
        if (format.empty()) return std::unexpected("The content is not a supported bitmap format.");
        if (format == "pnm") {
            auto pixels = readPnm(*bytes);
            if (!pixels) return std::unexpected(pixels.error());
            return DecodedBitmap{std::move(*pixels),format};
        }
        int width = 0,height = 0,channels = 0;
        const auto size = static_cast<int>(bytes->size());
        if (!stbi_info_from_memory(bytes->data(),size,&width,&height,&channels) || !validDimensions(width,height))
            return std::unexpected("Bitmap dimensions or header are invalid or exceed the configured limits.");
        const std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> image{
                stbi_load_from_memory(bytes->data(),size,&width,&height,&channels,4),stbi_image_free};
        if (!image) return std::unexpected("The bitmap decoder returned no pixels.");
        if (!validDimensions(width,height)) return std::unexpected("Decoded dimensions exceed the configured limits.");
        auto pixels = makeRasterLayer(width,height);
        for (std::size_t i = 0;i < pixels.pixels.size();++i) {
            const auto *rgba = image.get() + i * 4;
            pixels.pixels[i] = static_cast<std::uint32_t>(rgba[3]) << 24 | static_cast<std::uint32_t>(rgba[0]) << 16
                    | static_cast<std::uint32_t>(rgba[1]) << 8 | rgba[2];
        }
        return DecodedBitmap{std::move(pixels),format};
    } catch (const std::exception &e) { return std::unexpected(std::string(e.what())); }
}
std::expected<void,std::string> write(const std::filesystem::path &path,std::string_view requested,
                                     const RasterLayer &pixels,const BitmapFileWriteOptions &options)
{
    try {
        const auto format = normalizeFormat(requested);
        if (std::ranges::find(supportedBitmapWriteFormats(),format) == supportedBitmapWriteFormats().end())
            return std::unexpected("Unsupported bitmap writer: " + format);
        if (!validDimensions(pixels.width,pixels.height)
                || pixels.pixels.size() != static_cast<std::size_t>(pixels.width) * pixels.height)
            return std::unexpected("Invalid bitmap pixels.");
        if (options.quality < -1 || options.quality > 100) return std::unexpected("Quality must be -1 or within [0,100].");
        const bool opaque = format == "jpeg" || format == "pnm" || format == "hdr";
        const int channels = opaque ? 3 : 4;
        Bytes bytes(pixels.pixels.size() * channels);
        for (std::size_t i = 0;i < pixels.pixels.size();++i) {
            const auto argb = pixels.pixels[i]; const auto alpha = (argb >> 24) & 255U;
            for (unsigned c = 0;c < 3;++c) {
                const auto shift = (2 - c) * 8;
                const auto foreground = (argb >> shift) & 255U,background = (options.backgroundArgb >> shift) & 255U;
                bytes[i * channels + c] = static_cast<unsigned char>(opaque
                        ? (foreground * alpha + background * (255 - alpha) + 127) / 255 : foreground);
            }
            if (!opaque) bytes[i * 4 + 3] = static_cast<unsigned char>(alpha);
        }
        Output output;
        int ok = 0;
        if (format == "png") ok = stbi_write_png_to_func(appendBytes,&output,pixels.width,pixels.height,channels,bytes.data(),pixels.width*channels);
        else if (format == "jpeg") ok = stbi_write_jpg_to_func(appendBytes,&output,pixels.width,pixels.height,channels,bytes.data(),options.quality < 0 ? 90 : options.quality);
        else if (format == "bmp") ok = stbi_write_bmp_to_func(appendBytes,&output,pixels.width,pixels.height,channels,bytes.data());
        else if (format == "tga") ok = stbi_write_tga_to_func(appendBytes,&output,pixels.width,pixels.height,channels,bytes.data());
        else if (format == "hdr") {
            std::vector<float> linear(bytes.size());
            std::ranges::transform(bytes,linear.begin(),[](unsigned char b) { return static_cast<float>(std::pow(b / 255.0,2.2)); });
            ok = stbi_write_hdr_to_func(appendBytes,&output,pixels.width,pixels.height,3,linear.data());
        } else if (format == "pnm") {
            const auto header = "P6\n" + std::to_string(pixels.width) + " " + std::to_string(pixels.height) + "\n255\n";
            output.bytes.assign(header.begin(),header.end());
            output.bytes.insert(output.bytes.end(),bytes.begin(),bytes.end()); ok = 1;
        }
        if (!ok || output.failed || output.bytes.empty()) return std::unexpected("Bitmap encoding failed.");
        return writeBytes(path,output.bytes);
    } catch (const std::exception &e) { return std::unexpected(std::string(e.what())); }
}
}
