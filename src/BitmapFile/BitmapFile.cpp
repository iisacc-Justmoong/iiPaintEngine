#include "BitmapFile.h"
#include "BitmapCodec.hpp"
#include "Render/DirtyRegion.h"
#include <algorithm>
#include <utility>

const std::vector<std::string> &supportedBitmapReadFormats()
{
    static const std::vector<std::string> formats{"bmp","gif","hdr","jpeg","pic","png","pnm","psd","tga"};
    return formats;
}
const std::vector<std::string> &supportedBitmapWriteFormats()
{
    static const std::vector<std::string> formats{"bmp","hdr","jpeg","png","pnm","tga"};
    return formats;
}
const std::vector<std::string> &supportedEditableBitmapFormats() { return supportedBitmapWriteFormats(); }

bool BitmapFile::create(const std::filesystem::path &path, Types::Pixel width, Types::Pixel height,
                        std::string_view format, std::uint32_t clearArgb)
{
    if (path.empty() || !bitmap_codec::validDimensions(width,height)) {
        setError("Invalid bitmap path or dimensions."); return false;
    }
    const auto resolved = bitmap_codec::normalizeFormat(format.empty() ? path.extension().string() : std::string(format));
    if (std::ranges::find(supportedBitmapWriteFormats(),resolved) == supportedBitmapWriteFormats().end()) {
        setError("Unsupported bitmap writer: " + resolved); return false;
    }
    try {
        BitmapFile candidate;
        candidate.m_filePath = path; candidate.m_fileFormat = resolved;
        candidate.m_pixels = makeRasterLayer(width,height,clearArgb);
        candidate.m_open = true; candidate.m_modified = true;
        if (!candidate.save()) { setError(candidate.lastError()); return false; }
        *this = std::move(candidate); return true;
    } catch (const std::exception &e) { setError(e.what()); return false; }
}
bool BitmapFile::open(const std::filesystem::path &path)
{
    if (path.empty()) { setError("Bitmap path is empty."); return false; }
    auto decoded = bitmap_codec::read(path);
    if (!decoded) { setError(decoded.error()); return false; }
    m_filePath = path; m_fileFormat = std::move(decoded->format); m_pixels = std::move(decoded->pixels);
    m_open = true; m_modified = false; setError({}); return true;
}
bool BitmapFile::save(const BitmapFileWriteOptions &options)
{
    if (!canSaveInPlace()) { setError("No writable bitmap is open; select a supported saveAs format."); return false; }
    auto result = bitmap_codec::write(m_filePath,m_fileFormat,m_pixels,options);
    if (!result) { setError(result.error()); return false; }
    m_modified = false; setError({}); return true;
}
bool BitmapFile::saveAs(const std::filesystem::path &path, std::string_view format,
                        const BitmapFileWriteOptions &options)
{
    if (!isOpen() || path.empty()) { setError("No bitmap is open or output path is empty."); return false; }
    const auto resolved = bitmap_codec::normalizeFormat(format.empty() ? path.extension().string() : std::string(format));
    auto result = bitmap_codec::write(path,resolved,m_pixels,options);
    if (!result) { setError(result.error()); return false; }
    m_filePath = path; m_fileFormat = resolved; m_modified = false; setError({}); return true;
}
void BitmapFile::close()
{
    m_filePath.clear(); m_fileFormat.clear(); m_lastError.clear(); m_pixels = {}; m_open = false; m_modified = false;
}
bool BitmapFile::isOpen() const { return m_open && m_pixels.width > 0 && m_pixels.height > 0; }
bool BitmapFile::isModified() const { return m_modified; }
bool BitmapFile::isPixelWritable() const { return isOpen(); }
bool BitmapFile::canSaveInPlace() const
{
    return isOpen() && std::ranges::find(supportedBitmapWriteFormats(),m_fileFormat) != supportedBitmapWriteFormats().end();
}
const std::filesystem::path &BitmapFile::filePath() const { return m_filePath; }
const std::string &BitmapFile::fileFormat() const { return m_fileFormat; }
const std::string &BitmapFile::lastError() const { return m_lastError; }
Types::Pixel BitmapFile::width() const { return m_pixels.width; }
Types::Pixel BitmapFile::height() const { return m_pixels.height; }
const RasterLayer &BitmapFile::pixels() const { return m_pixels; }

bool BitmapFile::clear(std::uint32_t argb)
{
    if (!isPixelWritable()) {
        setError("No writable bitmap pixels are open.");
        return false;
    }
    std::fill(m_pixels.pixels.begin(), m_pixels.pixels.end(), argb);
    m_modified = true;
    setError({});
    return true;
}

bool BitmapFile::setPixel(DevicePixelPoint position, std::uint32_t argb)
{
    if (!isPixelWritable()
            || position.x < 0
            || position.y < 0
            || position.x >= m_pixels.width
            || position.y >= m_pixels.height) {
        setError("Bitmap pixel position is outside the open file.");
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(position.y)
            * static_cast<std::size_t>(m_pixels.width)
            + static_cast<std::size_t>(position.x);
    m_pixels.pixels[index] = argb;
    m_modified = true;
    setError({});
    return true;
}

bool BitmapFile::replacePixels(const RasterLayer &pixels)
{
    const std::size_t expected = static_cast<std::size_t>(std::max<Types::Pixel>(0, pixels.width))
            * static_cast<std::size_t>(std::max<Types::Pixel>(0, pixels.height));
    if (!isOpen() || !bitmap_codec::validDimensions(pixels.width,pixels.height) || pixels.pixels.size() != expected) {
        setError("Replacement bitmap pixels are invalid.");
        return false;
    }
    m_pixels = pixels;
    m_modified = true;
    setError({});
    return true;
}

bool BitmapFile::replacePixelPatch(DevicePixelRect bounds,
                                   const std::vector<std::uint32_t> &argbPixels)
{
    const DevicePixelRect fileBounds{{0, 0}, m_pixels.width, m_pixels.height};
    const DevicePixelRect clipped = intersectDevicePixelRects(fileBounds, bounds);
    if (!isOpen()
            || isEmpty(bounds)
            || clipped.origin.x != bounds.origin.x
            || clipped.origin.y != bounds.origin.y
            || clipped.width != bounds.width
            || clipped.height != bounds.height
            || argbPixels.size() != static_cast<std::size_t>(bounds.width)
                    * static_cast<std::size_t>(bounds.height)) {
        setError("Replacement bitmap patch is invalid.");
        return false;
    }

    for (Types::Pixel y = 0; y < bounds.height; ++y) {
        const std::size_t sourceRow = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(bounds.width);
        const std::size_t destinationRow = static_cast<std::size_t>(bounds.origin.y + y)
                * static_cast<std::size_t>(m_pixels.width)
                + static_cast<std::size_t>(bounds.origin.x);
        std::copy(argbPixels.begin() + static_cast<std::ptrdiff_t>(sourceRow),
                  argbPixels.begin() + static_cast<std::ptrdiff_t>(sourceRow + bounds.width),
                  m_pixels.pixels.begin() + static_cast<std::ptrdiff_t>(destinationRow));
    }
    m_modified = true;
    setError({});
    return true;
}

bool BitmapFile::applyStrokePixels(const StrokeCompositeBuffer &stroke, bool destinationOut)
{
    if (!isOpen() || stroke.width != m_pixels.width || stroke.height != m_pixels.height
            || stroke.pixels.size() != m_pixels.pixels.size()) {
        setError("Stroke pixels do not match the open bitmap file.");
        return false;
    }
    if (destinationOut) {
        eraseStrokeBufferFromLayer(m_pixels, stroke);
    } else {
        compositeStrokeBufferOntoLayer(m_pixels, stroke);
    }
    m_modified = true;
    setError({});
    return true;
}

void BitmapFile::setError(const std::string &error)
{
    m_lastError = error;
}
