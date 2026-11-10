#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Core/PaintRect.h"
#include "Layer/RasterLayer.h"

inline constexpr std::uint64_t maxBitmapPixelCount = 64ULL * 1024 * 1024;
inline constexpr int maxBitmapDimension = 16384;
inline constexpr std::uint64_t maxBitmapEncodedBytes = 256ULL * 1024 * 1024;

struct BitmapFileWriteOptions {
    int quality = -1;
    std::uint32_t backgroundArgb = 0xFFFFFFFFU;
};

// The application owns file access. A successful write must publish complete
// bytes, and a failed write must preserve any previously published file.
class BitmapFileIO {
public:
    using Bytes = std::vector<unsigned char>;
    virtual ~BitmapFileIO() = default;
    virtual std::expected<Bytes,std::string> read(const std::filesystem::path &,std::size_t maximumBytes) = 0;
    virtual std::expected<void,std::string> write(const std::filesystem::path &,std::span<const unsigned char>) = 0;
};

const std::vector<std::string> &supportedBitmapReadFormats();

const std::vector<std::string> &supportedBitmapWriteFormats();

const std::vector<std::string> &supportedEditableBitmapFormats();

class BitmapFile final {
public:
    explicit BitmapFile(std::shared_ptr<BitmapFileIO> io = {}) noexcept : m_io(std::move(io)) {}
    const std::shared_ptr<BitmapFileIO> &fileIO() const noexcept { return m_io; }
    bool create(const std::filesystem::path &filePath,
                Types::Pixel width,
                Types::Pixel height,
                std::string_view format = {},
                std::uint32_t clearArgb = 0x00000000U);
    bool open(const std::filesystem::path &filePath);
    bool save(const BitmapFileWriteOptions &options = {});
    bool saveAs(const std::filesystem::path &filePath,
                std::string_view format = {},
                const BitmapFileWriteOptions &options = {});
    void close();

    bool isOpen() const;
    bool isModified() const;
    bool isPixelWritable() const;
    bool canSaveInPlace() const;
    const std::filesystem::path &filePath() const;
    const std::string &fileFormat() const;
    const std::string &lastError() const;
    Types::Pixel width() const;
    Types::Pixel height() const;
    const RasterLayer &pixels() const;

    bool clear(std::uint32_t argb = 0x00000000U);
    bool setPixel(DevicePixelPoint position, std::uint32_t argb);
    bool replacePixels(const RasterLayer &pixels);
    bool replacePixelPatch(DevicePixelRect bounds, const std::vector<std::uint32_t> &argbPixels);
    bool applyStrokePixels(const StrokeCompositeBuffer &stroke, bool destinationOut);

private:
    std::filesystem::path m_filePath;
    std::string m_fileFormat;
    std::string m_lastError;
    RasterLayer m_pixels;
    bool m_open = false;
    bool m_modified = false;
    std::shared_ptr<BitmapFileIO> m_io;

    void setError(const std::string &error);
};
