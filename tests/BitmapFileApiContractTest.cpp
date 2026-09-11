#include <QColor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMetaMethod>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>

#include <iostream>
#include <type_traits>

#include <iiPaintEngine>
#include <iiFileProvider.h>

namespace {

bool hasMethod(const QMetaObject *metaObject, const char *signature)
{
    return metaObject->indexOfMethod(QMetaObject::normalizedSignature(signature)) >= 0;
}

} // namespace

int main(int argc, char **argv)
{
    static_assert(!std::is_base_of_v<QObject, BitmapBrushConfig>);
    static_assert(!std::is_base_of_v<QObject, BitmapViewportConfig>);
    static_assert(!std::is_base_of_v<QObject, BitmapRuntimeConfig>);
    static_assert(!std::is_base_of_v<QObject, BitmapFileState>);

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    registerIipeQmlTypes();

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick 2.15
import iipe 1.0 as Iipe

Iipe.BitmapFile {
    width: 64
    height: 48
    toolMode: "brush"
}
)", QUrl{"test://BitmapFileApiContract.qml"});

    QElapsedTimer timer;
    timer.start();
    while (component.isLoading() && timer.elapsed() < 1000) {
        app.processEvents();
    }

    QObject *object = component.create();
    if (object == nullptr) {
        std::cerr << component.errorString().toStdString() << '\n';
        return 1;
    }

    const auto bitmap = qobject_cast<BitmapFileItem *>(object);
    if (bitmap == nullptr || bitmap->fileOpen() || bitmap->pixelWritable()) {
        delete object;
        return 2;
    }

    const QMetaObject *metaObject = bitmap->metaObject();
    if (!hasMethod(metaObject, "createFile(QString,int,int)")
            || !hasMethod(metaObject, "createFile(QString,int,int,QString)")
            || !hasMethod(metaObject, "openFile(QString)")
            || !hasMethod(metaObject, "save()")
            || !hasMethod(metaObject, "save(int)")
            || !hasMethod(metaObject, "saveAs(QString)")
            || !hasMethod(metaObject, "saveAs(QString,QString)")
            || !hasMethod(metaObject, "saveAs(QString,QString,int)")
            || !hasMethod(metaObject, "undo()")
            || !hasMethod(metaObject, "redo()")
            || !hasMethod(metaObject, "setBrushConfig(BitmapBrushConfig)")
            || !hasMethod(metaObject, "setViewportConfig(BitmapViewportConfig)")
            || !hasMethod(metaObject, "setRuntimeConfig(BitmapRuntimeConfig)")
            || metaObject->indexOfProperty("filePath") < 0
            || metaObject->indexOfProperty("fileFormat") < 0
            || metaObject->indexOfProperty("pixelWritable") < 0
            || metaObject->indexOfProperty("canSaveInPlace") < 0
            || metaObject->indexOfProperty("modified") < 0
            || metaObject->indexOfProperty("supportedEditableFormats") < 0
            || !bitmap->supportedOpenFormats().contains(QStringLiteral("png"))
            || !bitmap->supportedSaveFormats().contains(QStringLiteral("png"))
            || !bitmap->supportedEditableFormats().contains(QStringLiteral("png"))
            || bitmap->supportedOpenFormats().contains(QStringLiteral("svg"))) {
        delete object;
        return 3;
    }

    BitmapBrushConfig brush;
    brush.color = QColor{"#557799"};
    brush.size = 23.0;
    brush.flow = 0.42;
    brush.opacity = 0.64;
    brush.hardness = 0.71;
    brush.spacingRatio = 0.33;
    brush.pressureCurveMinimum = 0.2;
    brush.pressureCurveCenter = 0.6;
    brush.pressureCurveMaximum = 0.8;
    bitmap->setBrushConfig(brush);
    if (bitmap->brushConfig().color != QColor{"#557799"}
            || bitmap->brushConfig().size != 23.0
            || bitmap->brushConfig().spacingRatio != 0.33) {
        delete object;
        return 4;
    }

    BitmapViewportConfig viewport;
    viewport.documentX = 3.5;
    viewport.documentY = -4.25;
    viewport.zoom = 2.5;
    viewport.devicePixelRatio = 1.75;
    viewport.viewWidth = 64.0;
    viewport.viewHeight = 48.0;
    bitmap->setViewportConfig(viewport);
    if (bitmap->viewportConfig().documentX != 3.5
            || bitmap->bitmapDevicePixelRatio() != 1.75
            || bitmap->width() != 64.0
            || bitmap->height() != 48.0) {
        delete object;
        return 5;
    }

    QTemporaryDir directory;
    if (!directory.isValid()) {
        delete object;
        return 6;
    }

    const QString target = directory.filePath(QStringLiteral("working-file.png"));
    if (!bitmap->createFile(target, 12, 10, QStringLiteral("png"))
            || !bitmap->fileOpen()
            || !bitmap->pixelWritable()
            || !bitmap->canSaveInPlace()
            || bitmap->modified()
            || bitmap->filePath() != target
            || bitmap->fileFormat() != QStringLiteral("png")
            || bitmap->bitmapWidth() != 12
            || bitmap->bitmapHeight() != 10
            || bitmap->width() != 64.0
            || bitmap->height() != 48.0) {
        delete object;
        return 7;
    }

    const BitmapFileState state = bitmap->stateSnapshot();
    if (!state.open
            || !state.pixelWritable
            || !state.canSaveInPlace
            || state.modified
            || state.bitmapWidth != 12
            || state.bitmapHeight != 10
            || state.filePath != target
            || state.fileFormat != QStringLiteral("png")) {
        delete object;
        return 8;
    }

    QImage source(3, 2, QImage::Format_ARGB32);
    source.fill(QColor{"#112233"});
    source.setPixelColor(1, 0, QColor{"#44AA66"});
    const QString sourcePath = directory.filePath(QStringLiteral("source.png"));
    if (!source.save(sourcePath)
            || !bitmap->openFile(sourcePath)
            || bitmap->bitmapWidth() != 3
            || bitmap->bitmapHeight() != 2
            || bitmap->width() != 64.0
            || bitmap->height() != 48.0) {
        delete object;
        return 9;
    }

    const QString copyPath = directory.filePath(QStringLiteral("copy.webp"));
    const QString format = bitmap->supportedSaveFormats().contains(QStringLiteral("webp"))
            ? QStringLiteral("webp")
            : QStringLiteral("png");
    const QString resolvedCopyPath = format == QStringLiteral("webp")
            ? copyPath
            : directory.filePath(QStringLiteral("copy.png"));
    if (!bitmap->saveAs(resolvedCopyPath, format, 90)
            || bitmap->filePath() != resolvedCopyPath
            || bitmap->fileFormat() != format
            || bitmap->modified()) {
        delete object;
        return 10;
    }

    delete object;
    if (iiFileProvider::File::read(resolvedCopyPath).isEmpty()
        || !iiFileProvider::File::remove(resolvedCopyPath)) return 11;
    return 0;
}
