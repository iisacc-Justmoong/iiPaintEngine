#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>

#include <iostream>

#include "QtAdapter/IipeQmlTypes.h"
#include "QtAdapter/PaintCanvasItem.h"

namespace {

bool waitForComponent(QGuiApplication &app, QQmlComponent &component)
{
    QElapsedTimer timer;
    timer.start();
    while (component.isLoading() && timer.elapsed() < 1000) {
        app.processEvents();
    }
    return !component.isLoading();
}

void printQmlErrors(const QQmlComponent &component)
{
    std::cerr << component.errorString().toStdString() << '\n';
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    registerIipeQmlTypes();

    const QFileInfo qmlFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_MAIN_QML)};
    const QFileInfo executableFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_EXECUTABLE)};
    if (!qmlFile.isFile() || !executableFile.isFile() || !executableFile.isExecutable()) {
        return 1;
    }

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.addImportPath(QString::fromUtf8(IIPAINTENGINE_LVRS_QML_IMPORT_PATH));

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlFile.absoluteFilePath()));
    if (!waitForComponent(app, component) || component.isError()) {
        printQmlErrors(component);
        return 1;
    }

    QObject *root = component.create();
    if (root == nullptr) {
        printQmlErrors(component);
        return 1;
    }

    const auto canvas = root->findChild<PaintCanvasItem *>(QStringLiteral("demoCanvas"));
    const QObject *controls = root->findChild<QObject *>(QStringLiteral("paintControls"));
    const QObject *clearButton = root->findChild<QObject *>(QStringLiteral("clearButton"));
    const QObject *livePreviewToggle = root->findChild<QObject *>(QStringLiteral("livePreviewToggle"));
    const QObject *flowArgumentToggle = root->findChild<QObject *>(QStringLiteral("flowArgumentToggle"));
    const QObject *opacityArgumentToggle = root->findChild<QObject *>(QStringLiteral("opacityArgumentToggle"));
    const QObject *hardnessArgumentToggle = root->findChild<QObject *>(QStringLiteral("hardnessArgumentToggle"));
    const QObject *spacingArgumentToggle = root->findChild<QObject *>(QStringLiteral("spacingArgumentToggle"));
    if (root->objectName() != QStringLiteral("iiPaintEngineExampleWindow")
            || canvas == nullptr
            || controls == nullptr
            || clearButton == nullptr
            || livePreviewToggle == nullptr
            || flowArgumentToggle == nullptr
            || opacityArgumentToggle == nullptr
            || hardnessArgumentToggle == nullptr
            || spacingArgumentToggle == nullptr
            || !root->property("demoReady").toBool()) {
        delete root;
        return 1;
    }

    if (canvas->brushSize() != 18.0
            || canvas->brushFlow() != 0.78
            || canvas->brushOpacity() != 0.92
            || canvas->brushSpacingRatio() != 0.22
            || canvas->zoom() != 1.0
            || !canvas->livePreviewEnabled()
            || !canvas->multithreadedEventsEnabled()
            || !canvas->brushFlowEnabled()
            || !canvas->brushOpacityEnabled()
            || !canvas->brushHardnessEnabled()
            || !canvas->brushSpacingEnabled()) {
        delete root;
        return 1;
    }

    if (!root->setProperty("flowArgumentEnabled", false)
            || !root->setProperty("opacityArgumentEnabled", false)
            || !root->setProperty("hardnessArgumentEnabled", false)
            || !root->setProperty("spacingArgumentEnabled", false)
            || !QMetaObject::invokeMethod(root, "applyBrushSettings")
            || canvas->brushFlowEnabled()
            || canvas->brushOpacityEnabled()
            || canvas->brushHardnessEnabled()
            || canvas->brushSpacingEnabled()) {
        delete root;
        return 1;
    }

    delete root;
    return 0;
}
