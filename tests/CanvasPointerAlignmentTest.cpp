#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>

#include "QtAdapter/PaintCanvasItem.h"

namespace {

class PointerTestCanvas : public PaintCanvasItem {
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

int alphaAt(const QImage &image, int x, int y)
{
    return qAlpha(image.pixel(x, y));
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);

    PointerTestCanvas canvas;
    canvas.setWidth(120);
    canvas.setHeight(90);
    canvas.setCanvasDevicePixelRatio(2.0);
    canvas.setMultithreadedEventsEnabled(false);
    canvas.setBrush(5.0, QColor{"#101318"}, 1.0, 1.0);
    canvas.setBrushSpacingRatio(0.25);

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{20.0, 30.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    canvas.mousePressEvent(&press);

    QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                  QPointF{21.0, 30.0},
                                  Qt::NoButton,
                                  Qt::LeftButton);
    canvas.mouseMoveEvent(&move);

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     QPointF{22.0, 30.0},
                                     Qt::LeftButton,
                                     Qt::NoButton);
    canvas.mouseReleaseEvent(&release);

    if (canvas.strokeCount() != 1) {
        return 1;
    }

    QImage rendered{120, 90, QImage::Format_ARGB32_Premultiplied};
    rendered.fill(Qt::transparent);
    QPainter painter{&rendered};
    canvas.paint(&painter);
    painter.end();

    if (alphaAt(rendered, 20, 30) == 0
            || alphaAt(rendered, 22, 30) == 0
            || alphaAt(rendered, 40, 60) != 0) {
        return 1;
    }

    return 0;
}
