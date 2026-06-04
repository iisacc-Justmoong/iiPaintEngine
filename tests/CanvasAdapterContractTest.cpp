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

#include "QtAdapter/CanvasAdapter.h"
#include "QtAdapter/CanvasBrushConfig.h"
#include "QtAdapter/IipeQmlTypes.h"

namespace {

bool hasMethod(const QMetaObject *metaObject, const char *signature)
{
    return metaObject->indexOfMethod(QMetaObject::normalizedSignature(signature)) >= 0;
}

bool invokeBool(QObject *object, const char *method)
{
    bool result = false;
    QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(bool, result));
    return result;
}

bool invokeBool(QObject *object, const char *method, const QString &path)
{
    bool result = false;
    QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(bool, result), Q_ARG(QString, path));
    return result;
}

bool invokeNewCanvas(QObject *object, int width, int height)
{
    bool result = false;
    QMetaObject::invokeMethod(object,
                              "newCanvas",
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(int, width),
                              Q_ARG(int, height));
    return result;
}

QImage savedImage(const QString &path)
{
    QImage image(path);
    return image.convertToFormat(QImage::Format_ARGB32);
}

} // namespace

int main(int argc, char **argv)
{
    static_assert(!std::is_base_of_v<QObject, CanvasBrushConfig>);

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    registerIipeQmlTypes();

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick 2.15
import iipe 1.0 as Iipe

Iipe.CanvasAdapter {
    width: 16
    height: 12
    toolMode: "brush"
}
)",
                      QUrl{"test://CanvasAdapterContract.qml"});

    QElapsedTimer timer;
    timer.start();
    while (component.isLoading() && timer.elapsed() < 1000) {
        app.processEvents();
    }

    QObject *object = component.create();
    if (object == nullptr) {
        std::cerr << "status=" << static_cast<int>(component.status()) << '\n';
        std::cerr << component.errorString().toStdString() << '\n';
        return 1;
    }

    const auto canvas = qobject_cast<CanvasAdapter *>(object);
    if (canvas == nullptr || canvas->toolMode() != QStringLiteral("brush")) {
        delete object;
        return 2;
    }

    const QMetaObject *metaObject = canvas->metaObject();
    if (!hasMethod(metaObject, "newCanvas(int,int)")
            || !hasMethod(metaObject, "openRaster(QString)")
            || !hasMethod(metaObject, "saveToFile(QString)")
            || !hasMethod(metaObject, "undo()")
            || !hasMethod(metaObject, "redo()")
            || !hasMethod(metaObject, "setBrushConfig(CanvasBrushConfig)")
            || metaObject->indexOfProperty("brushConfig") < 0
            || metaObject->indexOfProperty("toolMode") < 0) {
        delete object;
        return 3;
    }

    canvas->setToolMode(QStringLiteral("eraser"));
    if (canvas->toolMode() != QStringLiteral("eraser")) {
        delete object;
        return 4;
    }

    CanvasBrushConfig brush;
    brush.color = QColor{"#557799"};
    brush.size = 23.0;
    brush.flow = 0.42;
    brush.opacity = 0.64;
    brush.hardness = 0.71;
    brush.spacingRatio = 0.33;
    brush.flowEnabled = false;
    brush.opacityEnabled = false;
    brush.hardnessEnabled = false;
    brush.spacingEnabled = false;
    brush.pressureCurveMinimum = 0.2;
    brush.pressureCurveCenter = 0.6;
    brush.pressureCurveMaximum = 0.8;
    brush.stabilizerStrength = 0.44;
    canvas->setBrushConfig(brush);

    const CanvasBrushConfig appliedBrush = canvas->brushConfig();
    if (appliedBrush.color != QColor{"#557799"}
            || appliedBrush.size != 23.0
            || appliedBrush.flow != 0.42
            || appliedBrush.opacity != 0.64
            || appliedBrush.hardness != 0.71
            || appliedBrush.spacingRatio != 0.33
            || appliedBrush.flowEnabled
            || appliedBrush.opacityEnabled
            || appliedBrush.hardnessEnabled
            || appliedBrush.spacingEnabled
            || appliedBrush.pressureCurveMinimum != 0.2
            || appliedBrush.pressureCurveCenter != 0.6
            || appliedBrush.pressureCurveMaximum != 0.8
            || appliedBrush.stabilizerStrength != 0.44
            || canvas->brushSize() != 23.0
            || canvas->brushColor() != QColor{"#557799"}
            || canvas->brushFlowEnabled()
            || canvas->brushOpacityEnabled()
            || canvas->brushHardnessEnabled()
            || canvas->brushSpacingEnabled()) {
        delete object;
        return 15;
    }

    if (!invokeNewCanvas(canvas, 12, 10)
            || canvas->width() != 12.0
            || canvas->height() != 10.0
            || canvas->strokeCount() != 0) {
        delete object;
        return 5;
    }

    QTemporaryDir directory;
    if (!directory.isValid()) {
        delete object;
        return 6;
    }

    const QString sourcePath = directory.filePath(QStringLiteral("source.png"));
    QImage source(3, 2, QImage::Format_ARGB32);
    source.fill(QColor{"#112233"});
    source.setPixel(1, 0, QColor{"#44AA66"}.rgba());
    if (!source.save(sourcePath)) {
        delete object;
        return 7;
    }

    const QString firstSavePath = directory.filePath(QStringLiteral("first-save.png"));
    if (!invokeBool(canvas, "openRaster", sourcePath)
            || canvas->width() != 3.0
            || canvas->height() != 2.0
            || !invokeBool(canvas, "saveToFile", firstSavePath)) {
        delete object;
        return 8;
    }

    const QImage firstSave = savedImage(firstSavePath);
    if (firstSave.size() != QSize{3, 2}
            || firstSave.pixel(1, 0) != QColor{"#44AA66"}.rgba()) {
        delete object;
        return 9;
    }

    const QString undoSavePath = directory.filePath(QStringLiteral("undo-save.png"));
    if (!invokeBool(canvas, "undo")
            || canvas->width() != 12.0
            || canvas->height() != 10.0
            || !invokeBool(canvas, "saveToFile", undoSavePath)) {
        delete object;
        return 10;
    }

    const QImage undoSave = savedImage(undoSavePath);
    if (undoSave.size() != QSize{12, 10}
            || undoSave.pixel(1, 0) != 0x00000000U) {
        delete object;
        return 11;
    }

    const QString redoSavePath = directory.filePath(QStringLiteral("redo-save.png"));
    if (!invokeBool(canvas, "redo")
            || canvas->width() != 3.0
            || canvas->height() != 2.0
            || !invokeBool(canvas, "saveToFile", redoSavePath)) {
        delete object;
        return 12;
    }

    const QImage redoSave = savedImage(redoSavePath);
    if (redoSave.size() != QSize{3, 2}
            || redoSave.pixel(1, 0) != QColor{"#44AA66"}.rgba()) {
        delete object;
        return 13;
    }

    if (invokeBool(canvas, "redo")) {
        delete object;
        return 14;
    }

    delete object;
    return 0;
}
