#include "QtAdapter/IipeQmlTypes.h"
#include "backend/runtime/appentry.h"

#include <QtPlugin>

#if defined(LVRS_USE_STATIC_QML_PLUGIN)
Q_IMPORT_PLUGIN(LVRSPlugin)
#endif

int main(int argc, char *argv[])
{
    registerIipeQmlTypes();

    lvrs::QmlAppLaunchSpec launchSpec;
    launchSpec.bootstrap.applicationName = QStringLiteral("iiPaintEngine Example");
    launchSpec.bootstrap.quickStyleName = QStringLiteral("Basic");
    launchSpec.moduleUri = QStringLiteral("IiPaintEngineExample");
    launchSpec.rootObject = QStringLiteral("Main");
    launchSpec.windowActivationPolicy = lvrs::QmlWindowActivationPolicy::ShowRaiseAndActivate;

    return lvrs::runBootstrappedQmlApp(argc, argv, launchSpec);
}
