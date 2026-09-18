#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

#include <cstdint>
#include <vector>

#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

struct BitmapFileWriteOptions {
    int quality = -1;
    QColor backgroundColor = Qt::white;
};

QList<QByteArray> supportedBitmapReadFormats();

QList<QByteArray> supportedBitmapWriteFormats();

QList<QByteArray> supportedEditableBitmapFormats();

class BitmapFile final {
public:
    bool create(const QString &filePath,
                Types::Pixel width,
                Types::Pixel height,
                const QByteArray &format,
                std::uint32_t clearArgb = 0x00000000U);
    bool open(const QString &filePath);
    bool save(const BitmapFileWriteOptions &options = {});
    bool saveAs(const QString &filePath,
                const QByteArray &format = {},
                const BitmapFileWriteOptions &options = {});
    void close();

    bool isOpen() const;
    bool isModified() const;
    bool isPixelWritable() const;
    bool canSaveInPlace() const;
    QString filePath() const;
    QByteArray fileFormat() const;
    QString lastError() const;
    Types::Pixel width() const;
    Types::Pixel height() const;
    const RasterLayer &pixels() const;
    QImage image() const;

    bool clear(std::uint32_t argb = 0x00000000U);
    bool setPixel(DevicePixelPoint position, std::uint32_t argb);
    bool replacePixels(const RasterLayer &pixels);
    bool replacePixelPatch(DevicePixelRect bounds, const std::vector<std::uint32_t> &argbPixels);
    bool applyStrokePixels(const StrokeCompositeBuffer &stroke, bool destinationOut);

private:
    QString m_filePath;
    QByteArray m_fileFormat;
    QString m_lastError;
    RasterLayer m_pixels;
    bool m_open = false;
    bool m_modified = false;

    void setError(const QString &error);
};
