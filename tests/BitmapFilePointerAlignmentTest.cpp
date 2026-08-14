#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QTemporaryDir>

#include "QtAdapter/BitmapFileItem.h"

namespace {

class PointerTestBitmap : public BitmapFileItem {
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

int alphaAt(const QImage &image, int x, int y)
{
    return qAlpha(image.pixel(x, y));
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

    PointerTestBitmap bitmap;
    bitmap.setWidth(120);
    bitmap.setHeight(90);
    bitmap.setBitmapDevicePixelRatio(2.0);
    const QString filePath = directory.filePath(QStringLiteral("pointer.png"));
    if (!bitmap.createFile(filePath, 240, 180)) {
        return 1;
    }
    bitmap.setBrush(5.0, QColor{"#101318"}, 1.0, 1.0);
    bitmap.setBrushSpacingRatio(0.25);

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{20.0, 30.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    bitmap.mousePressEvent(&press);

    QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                  QPointF{21.0, 30.0},
                                  Qt::NoButton,
                                  Qt::LeftButton);
    bitmap.mouseMoveEvent(&move);

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     QPointF{22.0, 30.0},
                                     Qt::LeftButton,
                                     Qt::NoButton);
    bitmap.mouseReleaseEvent(&release);

    if (!waitForCommittedStroke(app, bitmap)) {
        return 1;
    }

    QImage rendered{120, 90, QImage::Format_ARGB32_Premultiplied};
    rendered.fill(Qt::transparent);
    QPainter painter{&rendered};
    bitmap.paint(&painter);
    painter.end();

    if (alphaAt(rendered, 20, 30) == 0
            || alphaAt(rendered, 22, 30) == 0
            || alphaAt(rendered, 40, 60) != 0) {
        return 1;
    }

    if (!bitmap.modified() || !bitmap.save() || bitmap.modified()) {
        return 1;
    }
    const QImage persisted{filePath};
    if (persisted.size() != QSize{240, 180}
            || alphaAt(persisted, 40, 60) == 0) {
        return 1;
    }

    return 0;
}
