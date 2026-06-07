#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QProcess>

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

bool executableHasRpath(const QString &executablePath, const QString &rpath)
{
    QProcess otool;
    otool.start(QStringLiteral("/usr/bin/otool"), {QStringLiteral("-l"), executablePath});
    if (!otool.waitForFinished(3000) || otool.exitStatus() != QProcess::NormalExit || otool.exitCode() != 0) {
        return false;
    }
    return QString::fromUtf8(otool.readAllStandardOutput()).contains(rpath);
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    registerIipeQmlTypes();

    const QFileInfo qmlFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_MAIN_QML)};
    const QFileInfo executableFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_EXECUTABLE)};
    const QFileInfo contractExecutableFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_DEMO_CONTRACT_EXECUTABLE)};
    if (!qmlFile.isFile()
            || !executableFile.isFile()
            || !executableFile.isExecutable()
            || !contractExecutableFile.isFile()
            || !contractExecutableFile.isExecutable()) {
        return 1;
    }
#if defined(__APPLE__)
    const QString executablePath = executableFile.absoluteFilePath();
    const QString contractExecutablePath = contractExecutableFile.absoluteFilePath();
    if (executablePath.contains(QStringLiteral(".app/Contents/MacOS/"))
            || executableFile.fileName() != QStringLiteral("iiPaintEngineExample")
            || executableFile.dir().dirName() != QStringLiteral("bin")) {
        return 1;
    }
    if (QString::fromUtf8(IIPAINTENGINE_LVRS_LIBRARY_DIR).isEmpty()
            || !executableHasRpath(executablePath, QString::fromUtf8(IIPAINTENGINE_LVRS_LIBRARY_DIR))
            || !executableHasRpath(contractExecutablePath, QString::fromUtf8(IIPAINTENGINE_LVRS_LIBRARY_DIR))) {
        return 1;
    }
#endif

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
    const QObject *spacingSlider = root->findChild<QObject *>(QStringLiteral("spacingSlider"));
    const QObject *pressureCurveGraph = root->findChild<QObject *>(QStringLiteral("pressureCurveGraph"));
    const QObject *pressureCurveMinimumSlider = root->findChild<QObject *>(QStringLiteral("pressureCurveMinimumSlider"));
    const QObject *pressureCurveCenterSlider = root->findChild<QObject *>(QStringLiteral("pressureCurveCenterSlider"));
    const QObject *pressureCurveMaximumSlider = root->findChild<QObject *>(QStringLiteral("pressureCurveMaximumSlider"));
    const QObject *stabilizerStrengthSlider = root->findChild<QObject *>(QStringLiteral("stabilizerStrengthSlider"));
    const QObject *previewFrameIntervalSlider = root->findChild<QObject *>(QStringLiteral("previewFrameIntervalSlider"));
    const QObject *inputPressureLabel = root->findChild<QObject *>(QStringLiteral("inputPressureLabel"));
    if (root->objectName() != QStringLiteral("iiPaintEngineExampleWindow")
            || canvas == nullptr
            || controls == nullptr
            || clearButton == nullptr
            || livePreviewToggle == nullptr
            || flowArgumentToggle == nullptr
            || opacityArgumentToggle == nullptr
            || hardnessArgumentToggle == nullptr
            || spacingArgumentToggle == nullptr
            || spacingSlider == nullptr
            || pressureCurveGraph == nullptr
            || pressureCurveMinimumSlider == nullptr
            || pressureCurveCenterSlider == nullptr
            || pressureCurveMaximumSlider == nullptr
            || stabilizerStrengthSlider == nullptr
            || previewFrameIntervalSlider == nullptr
            || inputPressureLabel == nullptr
            || !root->property("demoReady").toBool()) {
        delete root;
        return 1;
    }

    if (canvas->brushSize() != 18.0
            || canvas->brushFlow() != 0.78
            || canvas->brushOpacity() != 0.92
            || canvas->brushSpacingRatio() != 0.22
            || canvas->pressureCurveMinimum() != 0.0
            || canvas->pressureCurveCenter() != 0.5
            || canvas->pressureCurveMaximum() != 1.0
            || canvas->stabilizerStrength() != 0.25
            || canvas->livePreviewFrameIntervalMs() != 8
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

    if (spacingSlider->property("from").toReal() != 0.0
            || spacingSlider->property("to").toReal() != 1.0) {
        delete root;
        return 1;
    }

    if (pressureCurveMinimumSlider->property("from").toReal() != 0.0
            || pressureCurveMinimumSlider->property("to").toReal() != 1.0
            || pressureCurveCenterSlider->property("from").toReal() != 0.0
            || pressureCurveCenterSlider->property("to").toReal() != 1.0
            || pressureCurveMaximumSlider->property("from").toReal() != 0.0
            || pressureCurveMaximumSlider->property("to").toReal() != 1.0
            || stabilizerStrengthSlider->property("from").toReal() != 0.0
            || stabilizerStrengthSlider->property("to").toReal() != 1.0
            || previewFrameIntervalSlider->property("from").toReal() != 0.0
            || previewFrameIntervalSlider->property("to").toReal() != 33.0) {
        delete root;
        return 1;
    }

    if (!root->setProperty("currentPressureCurveMinimum", 0.2)
            || !root->setProperty("currentPressureCurveCenter", 0.6)
            || !root->setProperty("currentPressureCurveMaximum", 0.9)
            || !root->setProperty("currentStabilizerStrength", 0.75)
            || !root->setProperty("currentLivePreviewFrameIntervalMs", 12)
            || !QMetaObject::invokeMethod(root, "applyBrushSettings")
            || canvas->pressureCurveMinimum() != 0.2
            || canvas->pressureCurveCenter() != 0.6
            || canvas->pressureCurveMaximum() != 0.9
            || canvas->stabilizerStrength() != 0.75
            || canvas->livePreviewFrameIntervalMs() != 12) {
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
