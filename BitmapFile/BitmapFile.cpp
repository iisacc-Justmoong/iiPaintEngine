#include "BitmapFile.h"

#include <QColorSpace>
#include <iiFileProvider.h>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSet>

#include <algorithm>
#include <cstddef>
#include <utility>

#include "Render/DirtyRegion.h"

namespace {

QByteArray normalizedFormat(QByteArray format)
{
    format = format.trimmed().toLower();
    if (format == "jpg" || format == "jfif") {
        return "jpeg";
    }
    if (format == "tif") {
        return "tiff";
    }
    return format;
}

bool isVectorDocumentFormat(const QByteArray &format)
{
    static const QSet<QByteArray> vectorFormats{
            "eps",
            "pdf",
            "ps",
            "svg",
            "svgz",
    };
    return vectorFormats.contains(normalizedFormat(format));
}

bool hasVectorDocumentSignature(const QString &filePath)
{
    QByteArray prefix;
    try { prefix = iiFileProvider::File::readPrefix(filePath, 64 * 1024); }
    catch (const iiFileProvider::FileError &) { return false; }
    if (prefix.startsWith("\x1F\x8B")
            || prefix.startsWith("%PDF-")
            || prefix.startsWith("%!PS")) {
        return true;
    }
    prefix = prefix.trimmed().toLower();
    if (prefix.startsWith("\xEF\xBB\xBF")) {
        prefix.remove(0, 3);
        prefix = prefix.trimmed();
    }
    return prefix.startsWith('<')
            && (prefix.contains("<svg") || prefix.contains("<!doctype svg"));
}

QList<QByteArray> bitmapFormats(const QList<QByteArray> &formats)
{
    QList<QByteArray> result;
    for (const QByteArray &format : formats) {
        const QByteArray normalized = normalizedFormat(format);
        if (!normalized.isEmpty()
                && !isVectorDocumentFormat(normalized)
                && !result.contains(normalized)) {
            result.push_back(normalized);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

QByteArray nativeWriterFormat(const QByteArray &requested)
{
    const QByteArray normalized = normalizedFormat(requested);
    for (const QByteArray &candidate : QImageWriter::supportedImageFormats()) {
        if (normalizedFormat(candidate) == normalized
                && !isVectorDocumentFormat(candidate)) {
            return candidate.toLower();
        }
    }
    return {};
}

bool formatRequiresOpaquePixels(const QByteArray &format)
{
    static const QSet<QByteArray> opaqueFormats{
            "jpeg",
            "pbm",
            "pgm",
            "ppm",
            "wbmp",
            "xbm",
    };
    return opaqueFormats.contains(normalizedFormat(format));
}

QImage normalizedSrgbImage(const QImage &source)
{
    QImage image = source;
    if (image.colorSpace().isValid() && image.colorSpace() != QColorSpace::SRgb) {
        image = image.convertedToColorSpace(QColorSpace::SRgb);
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

RasterLayer rasterPixelsFromImage(const QImage &source)
{
    const QImage image = normalizedSrgbImage(source);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return {};
    }

    RasterLayer pixels = makeRasterLayer(image.width(), image.height());
    for (Types::Pixel y = 0; y < pixels.height; ++y) {
        for (Types::Pixel x = 0; x < pixels.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(pixels.width)
                    + static_cast<std::size_t>(x);
            pixels.pixels[index] = static_cast<std::uint32_t>(image.pixel(x, y));
        }
    }
    return pixels;
}

QImage imageFromRasterPixels(const RasterLayer &pixels)
{
    if (pixels.width <= 0 || pixels.height <= 0
            || pixels.pixels.size() != static_cast<std::size_t>(pixels.width)
                    * static_cast<std::size_t>(pixels.height)) {
        return {};
    }

    QImage image(pixels.width, pixels.height, QImage::Format_ARGB32);
    for (Types::Pixel y = 0; y < pixels.height; ++y) {
        for (Types::Pixel x = 0; x < pixels.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(pixels.width)
                    + static_cast<std::size_t>(x);
            image.setPixel(x, y, static_cast<QRgb>(pixels.pixels[index]));
        }
    }
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

QImage imageForWriter(const RasterLayer &pixels,
                      const QByteArray &format,
                      const QColor &backgroundColor)
{
    const QImage normalized = imageFromRasterPixels(pixels);
    if (!formatRequiresOpaquePixels(format)) {
        return normalized;
    }

    QImage opaque(normalized.size(), QImage::Format_RGB32);
    const QColor background = backgroundColor.isValid() ? backgroundColor : QColor{Qt::white};
    for (int y = 0; y < normalized.height(); ++y) {
        for (int x = 0; x < normalized.width(); ++x) {
            const QColor foreground = normalized.pixelColor(x, y);
            const int alpha = foreground.alpha();
            const int inverseAlpha = 255 - alpha;
            const int red = (foreground.red() * alpha + background.red() * inverseAlpha + 127) / 255;
            const int green = (foreground.green() * alpha + background.green() * inverseAlpha + 127) / 255;
            const int blue = (foreground.blue() * alpha + background.blue() * inverseAlpha + 127) / 255;
            opaque.setPixel(x, y, qRgb(red, green, blue));
        }
    }
    opaque.setColorSpace(QColorSpace::SRgb);
    return opaque;
}

QString writePixels(const QString &filePath,
                    const QByteArray &format,
                    const RasterLayer &pixels,
                    const BitmapFileWriteOptions &options)
{
    const QByteArray writerFormat = nativeWriterFormat(format);
    if (writerFormat.isEmpty()) {
        return QStringLiteral("No bitmap writer is installed for '%1'.")
                .arg(QString::fromLatin1(format));
    }

    try {
        iiFileProvider::File::writeWith(filePath, [&](QIODevice &output) {
            QImageWriter writer(&output, writerFormat);
            if (options.quality >= 0) writer.setQuality(std::clamp(options.quality, 0, 100));
            const QImage writableImage = imageForWriter(pixels, format, options.backgroundColor);
            if (writableImage.isNull() || !writer.write(writableImage))
                throw std::runtime_error(writer.errorString().isEmpty()
                    ? "The bitmap writer returned no output." : writer.errorString().toStdString());
        });
    } catch (const std::exception &error) { return QString::fromUtf8(error.what()); }
    return {};
}

} // namespace

QList<QByteArray> supportedBitmapReadFormats()
{
    return bitmapFormats(QImageReader::supportedImageFormats());
}

QList<QByteArray> supportedBitmapWriteFormats()
{
    return bitmapFormats(QImageWriter::supportedImageFormats());
}

QList<QByteArray> supportedEditableBitmapFormats()
{
    QList<QByteArray> editable;
    const QList<QByteArray> writable = supportedBitmapWriteFormats();
    for (const QByteArray &format : supportedBitmapReadFormats()) {
        if (writable.contains(format)) {
            editable.push_back(format);
        }
    }
    return editable;
}

bool BitmapFile::create(const QString &filePath,
                        Types::Pixel width,
                        Types::Pixel height,
                        const QByteArray &format,
                        std::uint32_t clearArgb)
{
    const QString path = filePath.trimmed();
    QByteArray resolvedFormat = normalizedFormat(format);
    if (resolvedFormat.isEmpty()) {
        resolvedFormat = normalizedFormat(QFileInfo{path}.suffix().toLatin1());
    }
    if (path.isEmpty()) {
        setError(QStringLiteral("Bitmap file path is empty."));
        return false;
    }
    if (width <= 0 || height <= 0) {
        setError(QStringLiteral("Bitmap dimensions must be positive."));
        return false;
    }
    if (!supportedBitmapWriteFormats().contains(resolvedFormat)) {
        setError(QStringLiteral("No bitmap writer is installed for '%1'.")
                         .arg(QString::fromLatin1(resolvedFormat)));
        return false;
    }

    BitmapFile candidate;
    candidate.m_filePath = path;
    candidate.m_fileFormat = resolvedFormat;
    candidate.m_pixels = makeRasterLayer(width, height, clearArgb);
    candidate.m_open = true;
    candidate.m_modified = true;
    if (!candidate.save()) {
        setError(candidate.lastError());
        return false;
    }

    *this = std::move(candidate);
    return true;
}

bool BitmapFile::open(const QString &filePath)
{
    const QString path = filePath.trimmed();
    if (path.isEmpty()) {
        setError(QStringLiteral("Bitmap file path is empty."));
        return false;
    }
    if (hasVectorDocumentSignature(path)) {
        setError(QStringLiteral("Vector documents are not accepted by the bitmap file boundary."));
        return false;
    }

    const QByteArray detectedFormat = normalizedFormat(QImageReader::imageFormat(path));
    if (detectedFormat.isEmpty()) {
        setError(QStringLiteral("The file is not a readable bitmap image."));
        return false;
    }
    if (isVectorDocumentFormat(detectedFormat)
            || !supportedBitmapReadFormats().contains(detectedFormat)) {
        setError(QStringLiteral("The detected format '%1' is not an allowed bitmap format.")
                         .arg(QString::fromLatin1(detectedFormat)));
        return false;
    }

    std::unique_ptr<QIODevice> input;
    try { input = iiFileProvider::File::openRead(path); }
    catch (const std::exception &error) { setError(QString::fromUtf8(error.what())); return false; }
    QImageReader reader(input.get());
    reader.setAutoDetectImageFormat(true);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    const QImage decoded = reader.read();
    if (decoded.isNull()) {
        setError(reader.errorString().isEmpty()
                         ? QStringLiteral("The bitmap decoder returned no pixels.")
                         : reader.errorString());
        return false;
    }

    RasterLayer pixels = rasterPixelsFromImage(decoded);
    if (pixels.width <= 0 || pixels.height <= 0) {
        setError(QStringLiteral("The bitmap could not be converted to ARGB32 sRGB pixels."));
        return false;
    }

    m_filePath = path;
    m_fileFormat = detectedFormat;
    m_pixels = std::move(pixels);
    m_open = true;
    m_modified = false;
    setError({});
    return true;
}

bool BitmapFile::save(const BitmapFileWriteOptions &options)
{
    if (!isOpen()) {
        setError(QStringLiteral("No bitmap file is open."));
        return false;
    }
    if (!canSaveInPlace()) {
        setError(QStringLiteral("No bitmap writer is installed for '%1'; use saveAs with a writable format.")
                         .arg(QString::fromLatin1(m_fileFormat)));
        return false;
    }

    const QString error = writePixels(m_filePath, m_fileFormat, m_pixels, options);
    if (!error.isEmpty()) {
        setError(error);
        return false;
    }
    m_modified = false;
    setError({});
    return true;
}

bool BitmapFile::saveAs(const QString &filePath,
                        const QByteArray &format,
                        const BitmapFileWriteOptions &options)
{
    if (!isOpen()) {
        setError(QStringLiteral("No bitmap file is open."));
        return false;
    }

    const QString path = filePath.trimmed();
    if (path.isEmpty()) {
        setError(QStringLiteral("Bitmap file path is empty."));
        return false;
    }
    QByteArray resolvedFormat = normalizedFormat(format);
    if (resolvedFormat.isEmpty()) {
        resolvedFormat = normalizedFormat(QFileInfo{path}.suffix().toLatin1());
    }
    if (!supportedBitmapWriteFormats().contains(resolvedFormat)) {
        setError(QStringLiteral("No bitmap writer is installed for '%1'.")
                         .arg(QString::fromLatin1(resolvedFormat)));
        return false;
    }

    const QString error = writePixels(path, resolvedFormat, m_pixels, options);
    if (!error.isEmpty()) {
        setError(error);
        return false;
    }
    m_filePath = path;
    m_fileFormat = resolvedFormat;
    m_modified = false;
    setError({});
    return true;
}

void BitmapFile::close()
{
    m_filePath.clear();
    m_fileFormat.clear();
    m_lastError.clear();
    m_pixels = {};
    m_open = false;
    m_modified = false;
}

bool BitmapFile::isOpen() const
{
    return m_open && m_pixels.width > 0 && m_pixels.height > 0;
}

bool BitmapFile::isModified() const
{
    return m_modified;
}

bool BitmapFile::isPixelWritable() const
{
    return isOpen();
}

bool BitmapFile::canSaveInPlace() const
{
    return isOpen() && supportedBitmapWriteFormats().contains(m_fileFormat);
}

QString BitmapFile::filePath() const
{
    return m_filePath;
}

QByteArray BitmapFile::fileFormat() const
{
    return m_fileFormat;
}

QString BitmapFile::lastError() const
{
    return m_lastError;
}

Types::Pixel BitmapFile::width() const
{
    return m_pixels.width;
}

Types::Pixel BitmapFile::height() const
{
    return m_pixels.height;
}

const RasterLayer &BitmapFile::pixels() const
{
    return m_pixels;
}

QImage BitmapFile::image() const
{
    return imageFromRasterPixels(m_pixels);
}

bool BitmapFile::clear(std::uint32_t argb)
{
    if (!isPixelWritable()) {
        setError(QStringLiteral("No writable bitmap pixels are open."));
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
        setError(QStringLiteral("Bitmap pixel position is outside the open file."));
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
    if (!isOpen() || pixels.width <= 0 || pixels.height <= 0 || pixels.pixels.size() != expected) {
        setError(QStringLiteral("Replacement bitmap pixels are invalid."));
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
        setError(QStringLiteral("Replacement bitmap patch is invalid."));
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
    if (!isOpen() || stroke.width != m_pixels.width || stroke.height != m_pixels.height) {
        setError(QStringLiteral("Stroke pixels do not match the open bitmap file."));
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

void BitmapFile::setError(const QString &error)
{
    m_lastError = error;
}
