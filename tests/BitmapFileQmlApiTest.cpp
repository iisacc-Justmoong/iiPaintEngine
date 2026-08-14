#include <QColor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMetaMethod>
#include <QQmlComponent>
#include <QQmlEngine>

#include <cstdlib>
#include <iostream>

#include "QtAdapter/IipeQmlTypes.h"
#include "QtAdapter/BitmapFileItem.h"

namespace {

bool hasMethod(const QMetaObject *metaObject, const char *signature)
{
    return metaObject->indexOfMethod(QMetaObject::normalizedSignature(signature)) >= 0;
}

} // namespace

int main(int argc, char **argv)
{
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
    height: 32
    documentX: 10
    documentY: 20
    zoom: 2
    bitmapDevicePixelRatio: 1.5
    brushColor: "#336699"
    brushSize: 9
    brushSpacing: 3
    brushSpacingRatio: 0
    brushFlow: 0.4
    brushFlowEnabled: false
    brushOpacity: 0.7
    brushOpacityEnabled: false
    brushHardness: 0.6
    brushHardnessEnabled: false
    brushSpacingEnabled: false
    pressureCurveMinimum: 0.2
    pressureCurveCenter: 0.6
    pressureCurveMaximum: 0.9
    livePreviewEnabled: true
}
)",
                      QUrl{"test://BitmapFileQmlApi.qml"});

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

    const auto bitmap = qobject_cast<BitmapFileItem *>(object);
    if (bitmap == nullptr) {
        delete object;
        return 1;
    }

    if (bitmap->documentX() != 10.0
            || bitmap->documentY() != 20.0
            || bitmap->zoom() != 2.0
            || bitmap->bitmapDevicePixelRatio() != 1.5
            || bitmap->brushColor() != QColor{"#336699"}
            || bitmap->brushSize() != 9.0
            || bitmap->brushSpacing() != 3.0
            || bitmap->brushSpacingRatio() != 0.0
            || bitmap->brushFlow() != 0.4
            || bitmap->brushFlowEnabled()
            || bitmap->brushOpacity() != 0.7
            || bitmap->brushOpacityEnabled()
            || bitmap->brushHardness() != 0.6
            || bitmap->brushHardnessEnabled()
            || bitmap->brushSpacingEnabled()
            || bitmap->pressureCurveMinimum() != 0.2
            || bitmap->pressureCurveCenter() != 0.6
            || bitmap->pressureCurveMaximum() != 0.9
            || !bitmap->livePreviewEnabled()
            || bitmap->liveStrokeActive()
            || bitmap->strokeCount() != 0
            || bitmap->inputDevice() != QStringLiteral("mouse")
            || bitmap->inputPressure() != 1.0) {
        delete object;
        return 1;
    }

    const QMetaObject *metaObject = bitmap->metaObject();
    if (!hasMethod(metaObject, "clear()")
            || !hasMethod(metaObject, "setDocumentViewport(qreal,qreal,qreal)")
            || !hasMethod(metaObject, "resetView()")
            || !hasMethod(metaObject, "panBy(qreal,qreal)")
            || !hasMethod(metaObject, "zoomAt(qreal,qreal,qreal)")
            || !hasMethod(metaObject, "setBrush(qreal,QColor,qreal,qreal)")) {
        delete object;
        return 1;
    }

    QMetaObject::invokeMethod(bitmap,
                              "setDocumentViewport",
                              Q_ARG(qreal, 3.0),
                              Q_ARG(qreal, 4.0),
                              Q_ARG(qreal, 1.25));
    QMetaObject::invokeMethod(bitmap,
                              "setBrush",
                              Q_ARG(qreal, 12.0),
                              Q_ARG(QColor, QColor{"#112233"}),
                              Q_ARG(qreal, 0.5),
                              Q_ARG(qreal, 0.75));
    if (bitmap->documentX() != 3.0
            || bitmap->documentY() != 4.0
            || bitmap->zoom() != 1.25
            || bitmap->brushSize() != 12.0
            || bitmap->brushColor() != QColor{"#112233"}
            || bitmap->brushFlow() != 0.5
            || bitmap->brushOpacity() != 0.75
            || bitmap->brushFlowEnabled()
            || bitmap->brushOpacityEnabled()
            || bitmap->brushHardnessEnabled()
            || bitmap->brushSpacingEnabled()) {
        delete object;
        return 1;
    }

    bitmap->setBrushFlowEnabled(true);
    bitmap->setBrushOpacityEnabled(true);
    bitmap->setBrushHardnessEnabled(true);
    bitmap->setBrushSpacingEnabled(true);
    bitmap->setBrushSpacingRatio(1.5);
    if (!bitmap->brushFlowEnabled()
            || !bitmap->brushOpacityEnabled()
            || !bitmap->brushHardnessEnabled()
            || !bitmap->brushSpacingEnabled()
            || bitmap->brushSpacingRatio() != 1.0) {
        delete object;
        return 1;
    }

    bitmap->setBrushSpacingRatio(0.0);
    if (bitmap->brushSpacingRatio() != 0.0) {
        delete object;
        return 1;
    }

    bitmap->setPressureCurveMinimum(0.7);
    bitmap->setPressureCurveCenter(0.1);
    bitmap->setPressureCurveMaximum(0.5);
    if (bitmap->pressureCurveMinimum() != 0.5
            || bitmap->pressureCurveCenter() != 0.5
            || bitmap->pressureCurveMaximum() != 0.5) {
        delete object;
        return 1;
    }

    QMetaObject::invokeMethod(bitmap, "clear");
    if (bitmap->strokeCount() != 0) {
        delete object;
        return 1;
    }

    delete object;
    return 0;
}
