#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
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

const std::vector<std::string> &supportedBitmapReadFormats();

const std::vector<std::string> &supportedBitmapWriteFormats();

const std::vector<std::string> &supportedEditableBitmapFormats();

class BitmapFile final {
public:
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

    void setError(const std::string &error);
};
