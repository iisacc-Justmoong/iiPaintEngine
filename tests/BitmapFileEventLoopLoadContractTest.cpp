#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointF>
#include <QTemporaryDir>

#include <iostream>

#include "QtAdapter/BitmapFileItem.h"

namespace {

class EventLoopTestBitmap : public BitmapFileItem {
public:
    using BitmapFileItem::mouseMoveEvent;
    using BitmapFileItem::mousePressEvent;
    using BitmapFileItem::mouseReleaseEvent;
};

QMouseEvent mouseEvent(QEvent::Type type,
                       QPointF position,
                       Qt::MouseButton button,
                       Qt::MouseButtons buttons)
{
    return QMouseEvent{type,
                       position,
                       position,
                       position,
                       button,
                       buttons,
                       Qt::NoModifier};
}

bool waitForPreview(QGuiApplication &app, BitmapFileItem &bitmap)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (bitmap.liveStrokeActive()) {
            return true;
        }
    }
    return false;
}

bool waitForCommittedStroke(QGuiApplication &app, BitmapFileItem &bitmap)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (bitmap.strokeCount() == 1 && !bitmap.liveStrokeActive()) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);

    QTemporaryDir directory;
    if (!directory.isValid()) {
        return 1;
    }

    EventLoopTestBitmap bitmap;
    bitmap.setWidth(256);
    bitmap.setHeight(128);
    bitmap.setBitmapDevicePixelRatio(1.0);
    if (!bitmap.createFile(directory.filePath(QStringLiteral("event-loop.png")), 256, 128)) {
        return 1;
    }
    bitmap.setLivePreviewEnabled(true);
    bitmap.setBrush(18.0, QColor{"#101318"}, 1.0, 1.0);

    const int initialQObjectChildren = bitmap.children().size();
    const int initialItemChildren = bitmap.childItems().size();
    if (initialQObjectChildren != 0 || initialItemChildren != 0) {
        std::cerr << "initial children qobject=" << initialQObjectChildren
                  << " item=" << initialItemChildren << '\n';
        return 1;
    }

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{24.0, 24.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    bitmap.mousePressEvent(&press);

    QPointF latestPosition{24.0, 24.0};
    for (int index = 0; index < 120; ++index) {
        latestPosition = QPointF{24.0 + static_cast<qreal>(index) * 1.5,
                                 24.0 + static_cast<qreal>(index % 7)};
        QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                      latestPosition,
                                      Qt::NoButton,
                                      Qt::LeftButton);
        bitmap.mouseMoveEvent(&move);
    }

    if (!waitForPreview(app, bitmap)) {
        std::cerr << "preview did not become active\n";
        return 1;
    }

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     latestPosition,
                                     Qt::LeftButton,
                                     Qt::NoButton);
    bitmap.mouseReleaseEvent(&release);
    if (!waitForCommittedStroke(app, bitmap)) {
        std::cerr << "commit wait failed strokeCount=" << bitmap.strokeCount()
                  << " live=" << bitmap.liveStrokeActive() << '\n';
        return 1;
    }

    if (bitmap.children().size() != initialQObjectChildren
            || bitmap.childItems().size() != initialItemChildren
            || bitmap.strokeCount() != 1) {
        std::cerr << "final children qobject=" << bitmap.children().size()
                  << " item=" << bitmap.childItems().size()
                  << " strokeCount=" << bitmap.strokeCount() << '\n';
        return 1;
    }

    return 0;
}
