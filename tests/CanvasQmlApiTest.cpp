#include <QColor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMetaMethod>
#include <QQmlComponent>
#include <QQmlEngine>

#include <cstdlib>
#include <iostream>

#include "QtAdapter/IipeQmlTypes.h"
#include "QtAdapter/PaintCanvasItem.h"

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

Iipe.Canvas {
    width: 64
    height: 32
    documentX: 10
    documentY: 20
    zoom: 2
    canvasDevicePixelRatio: 1.5
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
                      QUrl{"test://CanvasQmlApi.qml"});

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

    const auto canvas = qobject_cast<PaintCanvasItem *>(object);
    if (canvas == nullptr) {
        delete object;
        return 1;
    }

    if (canvas->documentX() != 10.0
            || canvas->documentY() != 20.0
            || canvas->zoom() != 2.0
            || canvas->canvasDevicePixelRatio() != 1.5
            || canvas->brushColor() != QColor{"#336699"}
            || canvas->brushSize() != 9.0
            || canvas->brushSpacing() != 3.0
            || canvas->brushSpacingRatio() != 0.0
            || canvas->brushFlow() != 0.4
            || canvas->brushFlowEnabled()
            || canvas->brushOpacity() != 0.7
            || canvas->brushOpacityEnabled()
            || canvas->brushHardness() != 0.6
            || canvas->brushHardnessEnabled()
            || canvas->brushSpacingEnabled()
            || canvas->pressureCurveMinimum() != 0.2
            || canvas->pressureCurveCenter() != 0.6
            || canvas->pressureCurveMaximum() != 0.9
            || !canvas->livePreviewEnabled()
            || canvas->liveStrokeActive()
            || canvas->strokeCount() != 0
            || canvas->inputDevice() != QStringLiteral("mouse")
            || canvas->inputPressure() != 1.0) {
        delete object;
        return 1;
    }

    const QMetaObject *metaObject = canvas->metaObject();
    if (!hasMethod(metaObject, "clear()")
            || !hasMethod(metaObject, "setDocumentViewport(qreal,qreal,qreal)")
            || !hasMethod(metaObject, "resetView()")
            || !hasMethod(metaObject, "panBy(qreal,qreal)")
            || !hasMethod(metaObject, "zoomAt(qreal,qreal,qreal)")
            || !hasMethod(metaObject, "setBrush(qreal,QColor,qreal,qreal)")) {
        delete object;
        return 1;
    }

    QMetaObject::invokeMethod(canvas,
                              "setDocumentViewport",
                              Q_ARG(qreal, 3.0),
                              Q_ARG(qreal, 4.0),
                              Q_ARG(qreal, 1.25));
    QMetaObject::invokeMethod(canvas,
                              "setBrush",
                              Q_ARG(qreal, 12.0),
                              Q_ARG(QColor, QColor{"#112233"}),
                              Q_ARG(qreal, 0.5),
                              Q_ARG(qreal, 0.75));
    if (canvas->documentX() != 3.0
            || canvas->documentY() != 4.0
            || canvas->zoom() != 1.25
            || canvas->brushSize() != 12.0
            || canvas->brushColor() != QColor{"#112233"}
            || canvas->brushFlow() != 0.5
            || canvas->brushOpacity() != 0.75
            || canvas->brushFlowEnabled()
            || canvas->brushOpacityEnabled()
            || canvas->brushHardnessEnabled()
            || canvas->brushSpacingEnabled()) {
        delete object;
        return 1;
    }

    canvas->setBrushFlowEnabled(true);
    canvas->setBrushOpacityEnabled(true);
    canvas->setBrushHardnessEnabled(true);
    canvas->setBrushSpacingEnabled(true);
    canvas->setBrushSpacingRatio(1.5);
    if (!canvas->brushFlowEnabled()
            || !canvas->brushOpacityEnabled()
            || !canvas->brushHardnessEnabled()
            || !canvas->brushSpacingEnabled()
            || canvas->brushSpacingRatio() != 1.0) {
        delete object;
        return 1;
    }

    canvas->setBrushSpacingRatio(0.0);
    if (canvas->brushSpacingRatio() != 0.0) {
        delete object;
        return 1;
    }

    canvas->setPressureCurveMinimum(0.7);
    canvas->setPressureCurveCenter(0.1);
    canvas->setPressureCurveMaximum(0.5);
    if (canvas->pressureCurveMinimum() != 0.5
            || canvas->pressureCurveCenter() != 0.5
            || canvas->pressureCurveMaximum() != 0.5) {
        delete object;
        return 1;
    }

    QMetaObject::invokeMethod(canvas, "clear");
    if (canvas->strokeCount() != 0) {
        delete object;
        return 1;
    }

    delete object;
    return 0;
}
