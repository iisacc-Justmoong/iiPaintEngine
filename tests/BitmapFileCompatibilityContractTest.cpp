#include <QByteArray>
#include <QFile>
#include <QDebug>
#include <QImageReader>
#include <QTemporaryDir>

#include <algorithm>

#include "BitmapFile/BitmapFile.h"
#include "Stroke/Rasterizer.h"

namespace {

bool containsFormat(const QList<QByteArray> &formats, const QByteArray &format)
{
    return std::find(formats.cbegin(), formats.cend(), format) != formats.cend();
}

} // namespace

int main()
{
    const QList<QByteArray> readable = supportedBitmapReadFormats();
    const QList<QByteArray> writable = supportedBitmapWriteFormats();
    if (!containsFormat(readable, "png")
            || !containsFormat(writable, "png")
            || containsFormat(readable, "svg")
            || containsFormat(readable, "svgz")
            || containsFormat(readable, "pdf")
            || containsFormat(writable, "svg")
            || containsFormat(writable, "pdf")) {
        return 1;
    }

    QTemporaryDir directory;
    if (!directory.isValid()) {
        return 2;
    }

    const QString extensionlessPath = directory.filePath(QStringLiteral("editable-bitmap"));
    BitmapFile bitmap;
    if (!bitmap.create(extensionlessPath, 7, 5, "png")
            || !bitmap.isOpen()
            || bitmap.isModified()
            || bitmap.filePath() != extensionlessPath
            || bitmap.fileFormat() != "png"
            || bitmap.width() != 7
            || bitmap.height() != 5) {
        return 3;
    }

    StrokeCompositeBuffer stroke = makeStrokeCompositeBuffer(7, 5);
    accumulateStrokeSamples(stroke, {RasterSample{{3, 2}, 0xFF2878C8U, 0xFFU}});
    if (!bitmap.applyStrokePixels(stroke, false)
            || !bitmap.isModified()
            || rasterLayerPixelAt(bitmap.pixels(), {3, 2}) != 0xFF2878C8U
            || !bitmap.save()
            || bitmap.isModified()) {
        return 4;
    }

    BitmapFile reopened;
    if (!reopened.open(extensionlessPath)
            || reopened.fileFormat() != "png"
            || rasterLayerPixelAt(reopened.pixels(), {3, 2}) != 0xFF2878C8U) {
        return 5;
    }

    const QString misleadingSuffix = directory.filePath(QStringLiteral("bitmap.svg"));
    if (!QFile::copy(extensionlessPath, misleadingSuffix)) {
        return 6;
    }
    BitmapFile contentDetected;
    if (!contentDetected.open(misleadingSuffix)
            || contentDetected.fileFormat() != "png"
            || !contentDetected.setPixel({1, 1}, 0xFFA05028U)
            || !contentDetected.save()
            || QImageReader::imageFormat(misleadingSuffix) != "png") {
        return 7;
    }

    const QString vectorPath = directory.filePath(QStringLiteral("forbidden.png"));
    QFile vectorFile(vectorPath);
    if (!vectorFile.open(QIODevice::WriteOnly)
            || vectorFile.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2\" height=\"2\"/>") <= 0) {
        return 8;
    }
    vectorFile.close();
    BitmapFile vectorDocument;
    if (vectorDocument.open(vectorPath) || vectorDocument.lastError().isEmpty()) {
        return 9;
    }

    for (const QByteArray &format : writable) {
        if (!containsFormat(readable, format)) {
            continue;
        }
        const QString path = directory.filePath(
                QStringLiteral("roundtrip-") + QString::fromLatin1(format));
        BitmapFile formatFile;
        if (!formatFile.create(path, 3, 2, format)
                || !formatFile.setPixel({1, 0}, 0xFF44AA66U)
                || !formatFile.save()) {
            return 10;
        }
        BitmapFile decoded;
        if (!decoded.open(path) || decoded.width() <= 0 || decoded.height() <= 0) {
            qCritical() << "runtime bitmap roundtrip failed" << format
                        << decoded.lastError() << decoded.width() << decoded.height();
            return 11;
        }
    }

    return 0;
}
