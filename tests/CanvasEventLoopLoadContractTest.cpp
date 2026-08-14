#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointF>

#include <iostream>

#include "QtAdapter/PaintCanvasItem.h"

namespace {

class EventLoopTestCanvas : public PaintCanvasItem {
public:
    using PaintCanvasItem::mouseMoveEvent;
    using PaintCanvasItem::mousePressEvent;
    using PaintCanvasItem::mouseReleaseEvent;
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

bool waitForPreview(QGuiApplication &app, PaintCanvasItem &canvas)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (canvas.liveStrokeActive()) {
            return true;
        }
    }
    return false;
}

bool waitForCommittedStroke(QGuiApplication &app, PaintCanvasItem &canvas)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (canvas.strokeCount() == 1 && !canvas.liveStrokeActive()) {
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

    EventLoopTestCanvas canvas;
    canvas.setWidth(256);
    canvas.setHeight(128);
    canvas.setCanvasDevicePixelRatio(1.0);
    canvas.setLivePreviewEnabled(true);
    canvas.setBrush(18.0, QColor{"#101318"}, 1.0, 1.0);

    const int initialQObjectChildren = canvas.children().size();
    const int initialItemChildren = canvas.childItems().size();
    if (initialQObjectChildren != 0 || initialItemChildren != 0) {
        std::cerr << "initial children qobject=" << initialQObjectChildren
                  << " item=" << initialItemChildren << '\n';
        return 1;
    }

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{24.0, 24.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    canvas.mousePressEvent(&press);

    QPointF latestPosition{24.0, 24.0};
    for (int index = 0; index < 120; ++index) {
        latestPosition = QPointF{24.0 + static_cast<qreal>(index) * 1.5,
                                 24.0 + static_cast<qreal>(index % 7)};
        QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                      latestPosition,
                                      Qt::NoButton,
                                      Qt::LeftButton);
        canvas.mouseMoveEvent(&move);
    }

    if (!waitForPreview(app, canvas)) {
        std::cerr << "preview did not become active\n";
        return 1;
    }

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     latestPosition,
                                     Qt::LeftButton,
                                     Qt::NoButton);
    canvas.mouseReleaseEvent(&release);
    if (!waitForCommittedStroke(app, canvas)) {
        std::cerr << "commit wait failed strokeCount=" << canvas.strokeCount()
                  << " live=" << canvas.liveStrokeActive() << '\n';
        return 1;
    }

    if (canvas.children().size() != initialQObjectChildren
            || canvas.childItems().size() != initialItemChildren
            || canvas.strokeCount() != 1) {
        std::cerr << "final children qobject=" << canvas.children().size()
                  << " item=" << canvas.childItems().size()
                  << " strokeCount=" << canvas.strokeCount() << '\n';
        return 1;
    }

    return 0;
}
