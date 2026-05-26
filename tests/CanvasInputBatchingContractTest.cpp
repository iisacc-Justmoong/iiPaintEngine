#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointF>

#include <iostream>

#include "QtAdapter/PaintCanvasItem.h"

namespace {

class InputBatchingTestCanvas : public PaintCanvasItem {
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

    InputBatchingTestCanvas canvas;
    canvas.setWidth(160);
    canvas.setHeight(96);
    canvas.setCanvasDevicePixelRatio(1.0);
    canvas.setMultithreadedEventsEnabled(false);
    canvas.setLivePreviewEnabled(true);
    canvas.setLivePreviewFrameIntervalMs(16);
    canvas.setBrush(18.0, QColor{"#101318"}, 1.0, 1.0);

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{20.0, 32.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    canvas.mousePressEvent(&press);

    QPointF latestPosition{20.0, 32.0};
    for (int index = 1; index <= 48; ++index) {
        latestPosition = QPointF{20.0 + static_cast<qreal>(index) * 2.0,
                                 32.0 + static_cast<qreal>(index % 5)};
        QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                      latestPosition,
                                      Qt::NoButton,
                                      Qt::LeftButton);
        canvas.mouseMoveEvent(&move);
    }

    if (canvas.liveStrokeActive() || canvas.strokeCount() != 0) {
        std::cerr << "input event synchronously created render work live="
                  << canvas.liveStrokeActive()
                  << " strokeCount=" << canvas.strokeCount() << '\n';
        return 1;
    }

    if (!waitForPreview(app, canvas)) {
        std::cerr << "batched preview did not render\n";
        return 1;
    }

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     latestPosition,
                                     Qt::LeftButton,
                                     Qt::NoButton);
    canvas.mouseReleaseEvent(&release);

    if (canvas.strokeCount() != 0) {
        std::cerr << "release synchronously committed strokeCount=" << canvas.strokeCount() << '\n';
        return 1;
    }

    if (!waitForCommittedStroke(app, canvas)) {
        std::cerr << "batched commit did not finish strokeCount=" << canvas.strokeCount() << '\n';
        return 1;
    }

    return 0;
}
