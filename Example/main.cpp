#include "QtAdapter/IipeQmlTypes.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QWindow>

#include <QtPlugin>

#if defined(LVRS_USE_STATIC_QML_PLUGIN)
Q_IMPORT_PLUGIN(LVRSPlugin)
#endif

void qml_register_types_LVRS();

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("iiPaintEngine Example"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    qml_register_types_LVRS();
    registerIipeQmlTypes();

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.loadFromModule(QStringLiteral("IiPaintEngineExample"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
        window->show();
        window->raise();
        window->requestActivate();
    }

    return app.exec();
}
