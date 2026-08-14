#include <QColor>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QWindow>
#include <QtGui/private/qeventpoint_p.h>

#include <algorithm>
#include <memory>

#include "QtAdapter/PaintCanvasItem.h"

namespace {

class TabletTestCanvas : public PaintCanvasItem {
public:
    using PaintCanvasItem::event;
    using PaintCanvasItem::mousePressEvent;
    using PaintCanvasItem::mouseMoveEvent;
    using PaintCanvasItem::mouseReleaseEvent;
};

QTabletEvent tabletEvent(QEvent::Type type,
                         QPointF position,
                         qreal pressure,
                         Qt::MouseButton button,
                         Qt::MouseButtons buttons)
{
    return QTabletEvent{
            type,
            QPointingDevice::primaryPointingDevice(),
            position,
            position,
            pressure,
            0.0F,
            0.0F,
            0.0F,
            0.0,
            0.0F,
            Qt::NoModifier,
            button,
            buttons,
    };
}

std::unique_ptr<QMouseEvent> synthesizedMouseEvent(QEvent::Type type,
                                                   QPointF position,
                                                   Qt::MouseButton button,
                                                   Qt::MouseButtons buttons,
                                                   qreal pressure = 1.0)
{
    auto event = std::make_unique<QMouseEvent>(type,
                                               position,
                                               position,
                                               position,
                                               button,
                                               buttons,
                                               Qt::NoModifier,
                                               Qt::MouseEventSynthesizedBySystem);
    QMutableEventPoint::setPressure(event->point(0), pressure);
    return event;
}

int alphaAt(const QImage &image, int x, int y)
{
    return qAlpha(image.pixel(x, y));
}

QImage renderCanvas(PaintCanvasItem &canvas)
{
    QImage rendered{100, 64, QImage::Format_ARGB32_Premultiplied};
    rendered.fill(Qt::transparent);
    QPainter painter{&rendered};
    canvas.paint(&painter);
    painter.end();
    return rendered;
}

int paintedPixelCount(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (alphaAt(image, x, y) > 0) {
                ++count;
            }
        }
    }
    return count;
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

QImage renderSinglePressureDab(QGuiApplication &app, qreal pressure)
{
    TabletTestCanvas canvas;
    canvas.setWidth(100);
    canvas.setHeight(64);
    canvas.setBrush(20.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushHardness(1.0);
    canvas.setBrushSpacingRatio(1.0);

    QTabletEvent press = tabletEvent(QEvent::TabletPress,
                                     QPointF{30.0, 30.0},
                                     pressure,
                                     Qt::NoButton,
                                     Qt::NoButton);
    canvas.event(&press);

    QTabletEvent release = tabletEvent(QEvent::TabletRelease,
                                       QPointF{30.0, 30.0},
                                       0.0,
                                       Qt::NoButton,
                                       Qt::NoButton);
    canvas.event(&release);

    if (!waitForCommittedStroke(app, canvas)) {
        return {};
    }

    return renderCanvas(canvas);
}

QImage renderSinglePressureDabWithSynthesizedMouse(QGuiApplication &app, qreal pressure)
{
    TabletTestCanvas canvas;
    canvas.setWidth(100);
    canvas.setHeight(64);
    canvas.setBrush(20.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushHardness(1.0);
    canvas.setBrushSpacingRatio(1.0);

    QTabletEvent press = tabletEvent(QEvent::TabletPress,
                                     QPointF{30.0, 30.0},
                                     pressure,
                                     Qt::NoButton,
                                     Qt::NoButton);
    canvas.event(&press);

    const std::unique_ptr<QMouseEvent> mousePress = synthesizedMouseEvent(QEvent::MouseButtonPress,
                                                                          QPointF{30.0, 30.0},
                                                                          Qt::LeftButton,
                                                                          Qt::LeftButton);
    canvas.mousePressEvent(mousePress.get());

    QTabletEvent release = tabletEvent(QEvent::TabletRelease,
                                       QPointF{30.0, 30.0},
                                       0.0,
                                       Qt::NoButton,
                                       Qt::NoButton);
    canvas.event(&release);

    const std::unique_ptr<QMouseEvent> mouseRelease = synthesizedMouseEvent(QEvent::MouseButtonRelease,
                                                                            QPointF{30.0, 30.0},
                                                                            Qt::LeftButton,
                                                                            Qt::NoButton);
    canvas.mouseReleaseEvent(mouseRelease.get());

    if (!waitForCommittedStroke(app, canvas)) {
        return {};
    }

    return renderCanvas(canvas);
}

QImage renderTabletStrokeStartedByPressureMove(QGuiApplication &app)
{
    TabletTestCanvas canvas;
    canvas.setWidth(100);
    canvas.setHeight(64);
    canvas.setBrush(20.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushHardness(1.0);
    canvas.setBrushSpacingRatio(0.25);

    QTabletEvent zeroPressurePress = tabletEvent(QEvent::TabletPress,
                                                 QPointF{20.0, 30.0},
                                                 0.0,
                                                 Qt::NoButton,
                                                 Qt::NoButton);
    canvas.event(&zeroPressurePress);

    QTabletEvent pressureMove = tabletEvent(QEvent::TabletMove,
                                            QPointF{40.0, 30.0},
                                            0.4,
                                            Qt::NoButton,
                                            Qt::NoButton);
    canvas.event(&pressureMove);

    QTabletEvent pressureRelease = tabletEvent(QEvent::TabletRelease,
                                               QPointF{60.0, 30.0},
                                               0.0,
                                               Qt::NoButton,
                                               Qt::NoButton);
    canvas.event(&pressureRelease);

    if (!waitForCommittedStroke(app, canvas)) {
        return {};
    }

    return renderCanvas(canvas);
}

QImage renderSynthesizedMouseFallback(QGuiApplication &app)
{
    TabletTestCanvas canvas;
    canvas.setWidth(100);
    canvas.setHeight(64);
    canvas.setBrush(16.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushHardness(1.0);
    canvas.setBrushSpacingRatio(0.25);

    const std::unique_ptr<QMouseEvent> mousePress = synthesizedMouseEvent(QEvent::MouseButtonPress,
                                                                          QPointF{30.0, 30.0},
                                                                          Qt::LeftButton,
                                                                          Qt::LeftButton);
    canvas.mousePressEvent(mousePress.get());

    const std::unique_ptr<QMouseEvent> mouseRelease = synthesizedMouseEvent(QEvent::MouseButtonRelease,
                                                                            QPointF{30.0, 30.0},
                                                                            Qt::LeftButton,
                                                                            Qt::NoButton);
    canvas.mouseReleaseEvent(mouseRelease.get());

    if (!waitForCommittedStroke(app, canvas)) {
        return {};
    }

    return renderCanvas(canvas);
}

QImage renderSynthesizedMousePressureDab(QGuiApplication &app, qreal pressure)
{
    TabletTestCanvas canvas;
    canvas.setWidth(100);
    canvas.setHeight(64);
    canvas.setBrush(20.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushHardness(1.0);
    canvas.setBrushSpacingRatio(1.0);

    const std::unique_ptr<QMouseEvent> mousePress = synthesizedMouseEvent(QEvent::MouseButtonPress,
                                                                          QPointF{30.0, 30.0},
                                                                          Qt::LeftButton,
                                                                          Qt::LeftButton,
                                                                          pressure);
    canvas.mousePressEvent(mousePress.get());

    const std::unique_ptr<QMouseEvent> mouseRelease = synthesizedMouseEvent(QEvent::MouseButtonRelease,
                                                                            QPointF{30.0, 30.0},
                                                                            Qt::LeftButton,
                                                                            Qt::NoButton,
                                                                            0.0);
    canvas.mouseReleaseEvent(mouseRelease.get());

    if (!waitForCommittedStroke(app, canvas)) {
        return {};
    }

    return renderCanvas(canvas);
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);

    TabletTestCanvas canvas;
    canvas.setWidth(100);
    canvas.setHeight(64);
    canvas.setBrush(12.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushHardness(1.0);
    canvas.setBrushSpacingRatio(0.25);

    QTabletEvent press = tabletEvent(QEvent::TabletPress,
                                     QPointF{20.0, 30.0},
                                     0.0,
                                     Qt::LeftButton,
                                     Qt::LeftButton);
    canvas.event(&press);

    QTabletEvent move = tabletEvent(QEvent::TabletMove,
                                    QPointF{40.0, 30.0},
                                    0.25,
                                    Qt::NoButton,
                                    Qt::LeftButton);
    canvas.event(&move);

    QTabletEvent release = tabletEvent(QEvent::TabletRelease,
                                       QPointF{60.0, 30.0},
                                       1.0,
                                       Qt::LeftButton,
                                       Qt::NoButton);
    canvas.event(&release);

    if (!waitForCommittedStroke(app, canvas)) {
        return 1;
    }

    const QImage rendered = renderCanvas(canvas);

    if (alphaAt(rendered, 60, 30) <= alphaAt(rendered, 20, 30)
            || alphaAt(rendered, 60, 30) == 0) {
        return 1;
    }

    const QImage lowPressureDab = renderSinglePressureDab(app, 0.2);
    const QImage highPressureDab = renderSinglePressureDab(app, 1.0);
    if (lowPressureDab.isNull()
            || highPressureDab.isNull()
            || alphaAt(highPressureDab, 30, 30) <= alphaAt(lowPressureDab, 30, 30)
            || alphaAt(lowPressureDab, 37, 30) != 0
            || alphaAt(highPressureDab, 37, 30) == 0
            || paintedPixelCount(highPressureDab) <= paintedPixelCount(lowPressureDab)) {
        return 1;
    }

    const QImage lowPressureWithSynthesizedMouse = renderSinglePressureDabWithSynthesizedMouse(app, 0.2);
    if (lowPressureWithSynthesizedMouse.isNull()
            || alphaAt(lowPressureWithSynthesizedMouse, 30, 30) != alphaAt(lowPressureDab, 30, 30)
            || alphaAt(lowPressureWithSynthesizedMouse, 37, 30) != 0
            || paintedPixelCount(lowPressureWithSynthesizedMouse) != paintedPixelCount(lowPressureDab)) {
        return 1;
    }

    const QImage lateContactStroke = renderTabletStrokeStartedByPressureMove(app);
    if (lateContactStroke.isNull()
            || alphaAt(lateContactStroke, 40, 30) == 0
            || paintedPixelCount(lateContactStroke) == 0) {
        return 1;
    }

    const QImage synthesizedFallbackStroke = renderSynthesizedMouseFallback(app);
    if (synthesizedFallbackStroke.isNull()
            || alphaAt(synthesizedFallbackStroke, 30, 30) == 0
            || paintedPixelCount(synthesizedFallbackStroke) == 0) {
        return 1;
    }

    const QImage lowPressureMouseDab = renderSynthesizedMousePressureDab(app, 0.2);
    const QImage highPressureMouseDab = renderSynthesizedMousePressureDab(app, 1.0);
    if (lowPressureMouseDab.isNull()
            || highPressureMouseDab.isNull()
            || alphaAt(highPressureMouseDab, 30, 30) <= alphaAt(lowPressureMouseDab, 30, 30)
            || alphaAt(lowPressureMouseDab, 37, 30) != 0
            || alphaAt(highPressureMouseDab, 37, 30) == 0
            || paintedPixelCount(highPressureMouseDab) <= paintedPixelCount(lowPressureMouseDab)) {
        return 1;
    }

    return 0;
}
