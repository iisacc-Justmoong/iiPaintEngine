#include <QCoreApplication>
#include <QGuiApplication>

#include "QtAdapter/BitmapFileItem.h"
#include "QtAdapter/HighFidelityPointerInput.h"

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, true);
    QCoreApplication::setAttribute(Qt::AA_CompressTabletEvents, true);
    configureHighFidelityPointerInput();
    if (!highFidelityPointerInputConfigured()
            || QCoreApplication::testAttribute(Qt::AA_CompressHighFrequencyEvents)
            || QCoreApplication::testAttribute(Qt::AA_CompressTabletEvents)) {
        return 1;
    }

    QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, true);
    QCoreApplication::setAttribute(Qt::AA_CompressTabletEvents, true);
    BitmapFileItem bitmap;
    return highFidelityPointerInputConfigured() ? 0 : 1;
}
