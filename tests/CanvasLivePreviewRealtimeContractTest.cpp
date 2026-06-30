#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>

#include <algorithm>
#include <cmath>

#include "QtAdapter/PaintCanvasItem.h"

namespace {

class LivePreviewTestCanvas : public PaintCanvasItem {
public:
    using PaintCanvasItem::mouseMoveEvent;
    using PaintCanvasItem::mousePressEvent;
    using PaintCanvasItem::mouseReleaseEvent;
    using PaintCanvasItem::redoRasterChange;
    using PaintCanvasItem::undoRasterChange;
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

QImage renderedCanvas(PaintCanvasItem &canvas);

bool hasPaintNear(PaintCanvasItem &canvas, QPointF position, int radius)
{
    const QImage rendered = renderedCanvas(canvas);
    const int centerX = static_cast<int>(std::lround(position.x()));
    const int centerY = static_cast<int>(std::lround(position.y()));
    for (int y = std::max(0, centerY - radius); y <= std::min(rendered.height() - 1, centerY + radius); ++y) {
        for (int x = std::max(0, centerX - radius); x <= std::min(rendered.width() - 1, centerX + radius); ++x) {
            if (qAlpha(rendered.pixel(x, y)) > 0) {
                return true;
            }
        }
    }
    return false;
}

QImage renderedCanvas(PaintCanvasItem &canvas)
{
    QImage rendered{160, 96, QImage::Format_ARGB32_Premultiplied};
    rendered.fill(Qt::transparent);
    QPainter painter{&rendered};
    canvas.paint(&painter);
    painter.end();
    return rendered;
}

int alphaAt(PaintCanvasItem &canvas, QPointF position)
{
    const QImage rendered = renderedCanvas(canvas);
    const int centerX = static_cast<int>(std::lround(position.x()));
    const int centerY = static_cast<int>(std::lround(position.y()));
    if (centerX < 0 || centerY < 0 || centerX >= rendered.width() || centerY >= rendered.height()) {
        return 0;
    }
    return qAlpha(rendered.pixel(centerX, centerY));
}

bool waitForLivePaint(QGuiApplication &app, PaintCanvasItem &canvas, QPointF position)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (hasPaintNear(canvas, position, 8)) {
            return true;
        }
    }
    return false;
}

bool waitForCommittedPaint(QGuiApplication &app, PaintCanvasItem &canvas, QPointF position)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (!canvas.liveStrokeActive() && hasPaintNear(canvas, position, 8)) {
            return true;
        }
    }
    return false;
}

bool waitForAlphaAtLeast(QGuiApplication &app, PaintCanvasItem &canvas, QPointF position, int minimumAlpha)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (alphaAt(canvas, position) >= minimumAlpha) {
            return true;
        }
    }
    return false;
}

bool waitForAlphaAtMost(QGuiApplication &app, PaintCanvasItem &canvas, QPointF position, int maximumAlpha)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (alphaAt(canvas, position) <= maximumAlpha) {
            return true;
        }
    }
    return false;
}

bool waitForCommittedAlphaAtMost(QGuiApplication &app,
                                 PaintCanvasItem &canvas,
                                 QPointF position,
                                 int maximumAlpha,
                                 int expectedStrokeCount)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (!canvas.liveStrokeActive()
                && canvas.strokeCount() == expectedStrokeCount
                && alphaAt(canvas, position) <= maximumAlpha) {
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

    LivePreviewTestCanvas canvas;
    canvas.setWidth(160);
    canvas.setHeight(96);
    canvas.setCanvasDevicePixelRatio(1.0);
    canvas.setMultithreadedEventsEnabled(true);
    canvas.setLivePreviewEnabled(true);
    canvas.setBrush(24.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushSpacingRatio(0.02);

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{12.0, 48.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    canvas.mousePressEvent(&press);

    QPointF latestPosition{12.0, 48.0};
    for (int i = 1; i <= 80; ++i) {
        latestPosition = QPointF{12.0 + static_cast<qreal>(i) * 1.6,
                                 48.0 + std::sin(static_cast<double>(i) * 0.2) * 12.0};
        QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                      latestPosition,
                                      Qt::NoButton,
                                      Qt::LeftButton);
        canvas.mouseMoveEvent(&move);
    }

    if (!waitForLivePaint(app, canvas, latestPosition)) {
        return 1;
    }

    if (!canvas.liveStrokeActive()) {
        return 1;
    }

    if (canvas.strokeCount() != 0) {
        return 1;
    }

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     latestPosition,
                                     Qt::LeftButton,
                                     Qt::NoButton);
    canvas.mouseReleaseEvent(&release);

    if (!hasPaintNear(canvas, latestPosition, 8)) {
        return 1;
    }

    if (canvas.strokeCount() != 0) {
        return 1;
    }

    if (!waitForCommittedPaint(app, canvas, latestPosition)) {
        return 1;
    }

    if (canvas.strokeCount() != 1) {
        return 1;
    }

    const int committedAlpha = alphaAt(canvas, latestPosition);
    if (committedAlpha <= 0
            || !canvas.undoRasterChange()
            || canvas.strokeCount() != 0
            || alphaAt(canvas, latestPosition) != 0
            || !canvas.redoRasterChange()
            || canvas.strokeCount() != 1
            || alphaAt(canvas, latestPosition) <= 0) {
        return 1;
    }

    LivePreviewTestCanvas eraserCanvas;
    eraserCanvas.setWidth(160);
    eraserCanvas.setHeight(96);
    eraserCanvas.setCanvasDevicePixelRatio(1.0);
    eraserCanvas.setMultithreadedEventsEnabled(true);
    eraserCanvas.setLivePreviewEnabled(true);
    eraserCanvas.setBrush(52.0, QColor{"#101318"}, 1.0, 1.0);
    eraserCanvas.setBrushSpacingRatio(0.0);
    eraserCanvas.setStabilizerStrength(0.0);

    QMouseEvent basePress = mouseEvent(QEvent::MouseButtonPress,
                                       QPointF{20.0, 48.0},
                                       Qt::LeftButton,
                                       Qt::LeftButton);
    eraserCanvas.mousePressEvent(&basePress);
    QMouseEvent baseMove = mouseEvent(QEvent::MouseMove,
                                      QPointF{140.0, 48.0},
                                      Qt::NoButton,
                                      Qt::LeftButton);
    eraserCanvas.mouseMoveEvent(&baseMove);
    QMouseEvent baseRelease = mouseEvent(QEvent::MouseButtonRelease,
                                         QPointF{140.0, 48.0},
                                         Qt::LeftButton,
                                         Qt::NoButton);
    eraserCanvas.mouseReleaseEvent(&baseRelease);

    const QPointF eraserPosition{80.0, 48.0};
    if (!waitForCommittedPaint(app, eraserCanvas, eraserPosition)
            || !waitForAlphaAtLeast(app, eraserCanvas, eraserPosition, 220)) {
        return 2;
    }

    eraserCanvas.setEraserMode(true);
    QMouseEvent eraserPress = mouseEvent(QEvent::MouseButtonPress,
                                         QPointF{60.0, 48.0},
                                         Qt::LeftButton,
                                         Qt::LeftButton);
    eraserCanvas.mousePressEvent(&eraserPress);
    for (int i = 1; i <= 20; ++i) {
        QMouseEvent eraserMove = mouseEvent(QEvent::MouseMove,
                                            QPointF{60.0 + static_cast<qreal>(i), 48.0},
                                            Qt::NoButton,
                                            Qt::LeftButton);
        eraserCanvas.mouseMoveEvent(&eraserMove);
    }

    if (!waitForAlphaAtMost(app, eraserCanvas, eraserPosition, 80)) {
        return 3;
    }

    if (!eraserCanvas.liveStrokeActive() || eraserCanvas.strokeCount() != 1) {
        return 4;
    }

    QMouseEvent eraserRelease = mouseEvent(QEvent::MouseButtonRelease,
                                           eraserPosition,
                                           Qt::LeftButton,
                                           Qt::NoButton);
    eraserCanvas.mouseReleaseEvent(&eraserRelease);
    if (!waitForCommittedAlphaAtMost(app, eraserCanvas, eraserPosition, 80, 2)) {
        return 5;
    }

    return 0;
}
