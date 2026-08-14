#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QProcess>

#include <iostream>

#include "QtAdapter/IipeQmlTypes.h"
#include "QtAdapter/BitmapFileItem.h"

void qml_register_types_LVRS();

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

int fail(const char *message)
{
    std::cerr << message << '\n';
    return 1;
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
    qml_register_types_LVRS();
    registerIipeQmlTypes();

    const QFileInfo qmlFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_MAIN_QML)};
    const QFileInfo executableFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_EXECUTABLE)};
    const QFileInfo contractExecutableFile{QString::fromUtf8(IIPAINTENGINE_EXAMPLE_DEMO_CONTRACT_EXECUTABLE)};
    if (!qmlFile.isFile()
            || !executableFile.isFile()
            || !executableFile.isExecutable()
            || !contractExecutableFile.isFile()
            || !contractExecutableFile.isExecutable()) {
        return fail("Example source or executable contract input is missing or not executable.");
    }

    QFile qmlSource{qmlFile.absoluteFilePath()};
    if (!qmlSource.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail("Example Main.qml could not be opened.");
    }
    QByteArray qmlBytes = qmlSource.readAll();
    qmlBytes.replace("\r\n", "\n");
    if (!qmlBytes.startsWith("pragma ComponentBehavior: Bound\n")
            || qmlBytes.contains("\npragma\nComponentBehavior: Bound")) {
        return fail("Example Main.qml has an invalid ComponentBehavior pragma layout.");
    }
#if defined(__APPLE__)
    const QString executablePath = executableFile.absoluteFilePath();
    const QString contractExecutablePath = contractExecutableFile.absoluteFilePath();
    if (executablePath.contains(QStringLiteral(".app/Contents/MacOS/"))
            || executableFile.fileName() != QStringLiteral("iiPaintEngineExample")
            || executableFile.dir().dirName() != QStringLiteral("bin")) {
        return fail("The macOS example executable layout is invalid.");
    }
    if (QString::fromUtf8(IIPAINTENGINE_LVRS_LIBRARY_DIR).isEmpty()
            || !executableHasRpath(executablePath, QString::fromUtf8(IIPAINTENGINE_LVRS_LIBRARY_DIR))
            || !executableHasRpath(contractExecutablePath, QString::fromUtf8(IIPAINTENGINE_LVRS_LIBRARY_DIR))) {
        return fail("The macOS example or contract executable is missing the LVRS rpath.");
    }
#endif

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.addImportPath(QString::fromUtf8(IIPAINTENGINE_LVRS_QML_IMPORT_PATH));

    QQmlComponent component(&engine, QUrl::fromLocalFile(qmlFile.absoluteFilePath()));
    if (!waitForComponent(app, component) || component.isError()) {
        printQmlErrors(component);
        return fail("Example Main.qml did not compile through QQmlComponent.");
    }

    QObject *root = component.create();
    if (root == nullptr) {
        printQmlErrors(component);
        return fail("Example Main.qml did not create a root object.");
    }

    const auto bitmap = root->findChild<BitmapFileItem *>(QStringLiteral("demoBitmap"));
    const QObject *controls = root->findChild<QObject *>(QStringLiteral("paintControls"));
    const QObject *clearButton = root->findChild<QObject *>(QStringLiteral("clearButton"));
    const QObject *livePreviewToggle = root->findChild<QObject *>(QStringLiteral("livePreviewToggle"));
    const QObject *sizeSlider = root->findChild<QObject *>(QStringLiteral("sizeSlider"));
    const QObject *flowSlider = root->findChild<QObject *>(QStringLiteral("flowSlider"));
    const QObject *flowArgumentToggle = root->findChild<QObject *>(QStringLiteral("flowArgumentToggle"));
    const QObject *opacitySlider = root->findChild<QObject *>(QStringLiteral("opacitySlider"));
    const QObject *opacityArgumentToggle = root->findChild<QObject *>(QStringLiteral("opacityArgumentToggle"));
    const QObject *pressureOpacityArgumentToggle = root->findChild<QObject *>(QStringLiteral("pressureOpacityArgumentToggle"));
    const QObject *hardnessSlider = root->findChild<QObject *>(QStringLiteral("hardnessSlider"));
    const QObject *hardnessArgumentToggle = root->findChild<QObject *>(QStringLiteral("hardnessArgumentToggle"));
    const QObject *spacingArgumentToggle = root->findChild<QObject *>(QStringLiteral("spacingArgumentToggle"));
    const QObject *spacingSlider = root->findChild<QObject *>(QStringLiteral("spacingSlider"));
    const QObject *pressureCurveGraph = root->findChild<QObject *>(QStringLiteral("pressureCurveGraph"));
    const QObject *pressureCurveMinimumSlider = root->findChild<QObject *>(QStringLiteral("pressureCurveMinimumSlider"));
    const QObject *pressureCurveCenterSlider = root->findChild<QObject *>(QStringLiteral("pressureCurveCenterSlider"));
    const QObject *pressureCurveMaximumSlider = root->findChild<QObject *>(QStringLiteral("pressureCurveMaximumSlider"));
    const QObject *inputPressureLabel = root->findChild<QObject *>(QStringLiteral("inputPressureLabel"));
    if (root->objectName() != QStringLiteral("iiPaintEngineExampleWindow")
            || bitmap == nullptr
            || controls == nullptr
            || clearButton == nullptr
            || livePreviewToggle == nullptr
            || sizeSlider == nullptr
            || flowSlider == nullptr
            || flowArgumentToggle == nullptr
            || opacitySlider == nullptr
            || opacityArgumentToggle == nullptr
            || pressureOpacityArgumentToggle == nullptr
            || hardnessSlider == nullptr
            || hardnessArgumentToggle == nullptr
            || spacingArgumentToggle == nullptr
            || spacingSlider == nullptr
            || pressureCurveGraph == nullptr
            || pressureCurveMinimumSlider == nullptr
            || pressureCurveCenterSlider == nullptr
            || pressureCurveMaximumSlider == nullptr
            || inputPressureLabel == nullptr
            || !root->property("demoReady").toBool()) {
        delete root;
        return fail("Example Main.qml is missing a required demo object or ready state.");
    }

    if (bitmap->brushSize() != 18.0
            || bitmap->brushFlow() != 1.0
            || bitmap->brushOpacity() != 1.0
            || bitmap->brushHardness() != 1.0
            || bitmap->brushSpacingRatio() != 0.0
            || bitmap->pressureCurveMinimum() != 0.0
            || bitmap->pressureCurveCenter() != 0.5
            || bitmap->pressureCurveMaximum() != 1.0
            || bitmap->zoom() != 1.0
            || !bitmap->livePreviewEnabled()
            || !bitmap->brushFlowEnabled()
            || !bitmap->brushOpacityEnabled()
            || !bitmap->pressureToOpacityEnabled()
            || !bitmap->brushHardnessEnabled()
            || !bitmap->brushSpacingEnabled()) {
        delete root;
        return fail("Example bitmap default brush or preview state is invalid.");
    }

    if (sizeSlider->property("from").toReal() != 2.0
            || sizeSlider->property("to").toReal() != 72.0
            || flowSlider->property("from").toReal() != 0.05
            || flowSlider->property("to").toReal() != 1.0
            || opacitySlider->property("from").toReal() != 0.05
            || opacitySlider->property("to").toReal() != 1.0
            || hardnessSlider->property("from").toReal() != 0.05
            || hardnessSlider->property("to").toReal() != 1.0
            || spacingSlider->property("from").toReal() != 0.0
            || spacingSlider->property("to").toReal() != 1.0) {
        delete root;
        return fail("Example primary slider ranges are invalid.");
    }

    if (pressureCurveMinimumSlider->property("from").toReal() != 0.0
            || pressureCurveMinimumSlider->property("to").toReal() != 1.0
            || pressureCurveCenterSlider->property("from").toReal() != 0.0
            || pressureCurveCenterSlider->property("to").toReal() != 1.0
            || pressureCurveMaximumSlider->property("from").toReal() != 0.0
            || pressureCurveMaximumSlider->property("to").toReal() != 1.0) {
        delete root;
        return fail("Example advanced slider ranges are invalid.");
    }

    if (!root->setProperty("currentPressureCurveMinimum", 0.2)
            || !root->setProperty("currentPressureCurveCenter", 0.6)
            || !root->setProperty("currentPressureCurveMaximum", 0.9)
            || !QMetaObject::invokeMethod(root, "applyBrushSettings")
            || bitmap->pressureCurveMinimum() != 0.2
            || bitmap->pressureCurveCenter() != 0.6
            || bitmap->pressureCurveMaximum() != 0.9) {
        delete root;
        return fail("Example brush-setting application contract failed.");
    }

    if (!root->setProperty("flowArgumentEnabled", false)
            || !root->setProperty("opacityArgumentEnabled", false)
            || !root->setProperty("pressureOpacityArgumentEnabled", false)
            || !root->setProperty("hardnessArgumentEnabled", false)
            || !root->setProperty("spacingArgumentEnabled", false)
            || !QMetaObject::invokeMethod(root, "applyBrushSettings")
            || bitmap->brushFlowEnabled()
            || bitmap->brushOpacityEnabled()
            || bitmap->pressureToOpacityEnabled()
            || bitmap->brushHardnessEnabled()
            || bitmap->brushSpacingEnabled()) {
        delete root;
        return fail("Example brush-setting feature-toggle contract failed.");
    }

    delete root;
    return 0;
}
