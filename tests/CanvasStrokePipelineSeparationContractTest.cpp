#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointF>

#include <iostream>

#include "QtAdapter/PaintCanvasItem.h"

namespace {

class PipelineSeparationTestCanvas : public PaintCanvasItem {
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

    PipelineSeparationTestCanvas canvas;
    canvas.setWidth(160);
    canvas.setHeight(96);
    canvas.setCanvasDevicePixelRatio(1.0);
    canvas.setMultithreadedEventsEnabled(false);
    canvas.setLivePreviewEnabled(true);
    canvas.setLivePreviewFrameIntervalMs(16);
    canvas.setBrush(16.0, QColor{"#101318"}, 1.0, 1.0);

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{24.0, 36.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    canvas.mousePressEvent(&press);

    for (int index = 1; index <= 24; ++index) {
        QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                      QPointF{24.0 + static_cast<qreal>(index), 36.0},
                                      Qt::NoButton,
                                      Qt::LeftButton);
        canvas.mouseMoveEvent(&move);
    }

    if (canvas.liveStrokeActive() || canvas.strokeCount() != 0) {
        std::cerr << "input event synchronously touched render or document state\n";
        return 1;
    }

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     QPointF{48.0, 36.0},
                                     Qt::LeftButton,
                                     Qt::NoButton);
    canvas.mouseReleaseEvent(&release);

    if (canvas.liveStrokeActive() || canvas.strokeCount() != 0) {
        std::cerr << "stroke release synchronously committed document state\n";
        return 1;
    }

    if (!waitForCommittedStroke(app, canvas)) {
        std::cerr << "deferred stroke commit did not complete\n";
        return 1;
    }

    return 0;
}
