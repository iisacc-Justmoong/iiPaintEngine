#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>

#include "QtAdapter/BitmapFileItem.h"

namespace {

class LivePreviewTestBitmap : public BitmapFileItem {
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

QImage renderedBitmap(BitmapFileItem &bitmap);

bool hasPaintNear(BitmapFileItem &bitmap, QPointF position, int radius)
{
    const QImage rendered = renderedBitmap(bitmap);
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

QImage renderedBitmap(BitmapFileItem &bitmap)
{
    QImage rendered{160, 96, QImage::Format_ARGB32_Premultiplied};
    rendered.fill(Qt::transparent);
    QPainter painter{&rendered};
    bitmap.paint(&painter);
    painter.end();
    return rendered;
}

int alphaAt(BitmapFileItem &bitmap, QPointF position)
{
    const QImage rendered = renderedBitmap(bitmap);
    const int centerX = static_cast<int>(std::lround(position.x()));
    const int centerY = static_cast<int>(std::lround(position.y()));
    if (centerX < 0 || centerY < 0 || centerX >= rendered.width() || centerY >= rendered.height()) {
        return 0;
    }
    return qAlpha(rendered.pixel(centerX, centerY));
}

bool waitForLivePaint(QGuiApplication &app, BitmapFileItem &bitmap, QPointF position)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (hasPaintNear(bitmap, position, 8)) {
            return true;
        }
    }
    return false;
}

bool waitForCommittedPaint(QGuiApplication &app, BitmapFileItem &bitmap, QPointF position)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (!bitmap.liveStrokeActive() && hasPaintNear(bitmap, position, 8)) {
            return true;
        }
    }
    return false;
}

bool waitForAlphaAtLeast(QGuiApplication &app, BitmapFileItem &bitmap, QPointF position, int minimumAlpha)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (alphaAt(bitmap, position) >= minimumAlpha) {
            return true;
        }
    }
    return false;
}

bool waitForAlphaAtMost(QGuiApplication &app, BitmapFileItem &bitmap, QPointF position, int maximumAlpha)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (alphaAt(bitmap, position) <= maximumAlpha) {
            return true;
        }
    }
    return false;
}

bool waitForCommittedAlphaAtMost(QGuiApplication &app,
                                 BitmapFileItem &bitmap,
                                 QPointF position,
                                 int maximumAlpha,
                                 int expectedStrokeCount)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        app.processEvents(QEventLoop::AllEvents, 10);
        if (!bitmap.liveStrokeActive()
                && bitmap.strokeCount() == expectedStrokeCount
                && alphaAt(bitmap, position) <= maximumAlpha) {
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

    LivePreviewTestBitmap bitmap;
    bitmap.setWidth(160);
    bitmap.setHeight(96);
    bitmap.setBitmapDevicePixelRatio(1.0);
    if (!bitmap.createFile(directory.filePath(QStringLiteral("live.png")), 160, 96)) {
        return 1;
    }
    bitmap.setLivePreviewEnabled(true);
    bitmap.setBrush(24.0, QColor{"#101318"}, 1.0, 1.0);
    bitmap.setBrushSpacingRatio(0.02);

    QMouseEvent press = mouseEvent(QEvent::MouseButtonPress,
                                   QPointF{12.0, 48.0},
                                   Qt::LeftButton,
                                   Qt::LeftButton);
    bitmap.mousePressEvent(&press);

    QPointF latestPosition{12.0, 48.0};
    for (int i = 1; i <= 80; ++i) {
        latestPosition = QPointF{12.0 + static_cast<qreal>(i) * 1.6,
                                 48.0 + std::sin(static_cast<double>(i) * 0.2) * 12.0};
        QMouseEvent move = mouseEvent(QEvent::MouseMove,
                                      latestPosition,
                                      Qt::NoButton,
                                      Qt::LeftButton);
        bitmap.mouseMoveEvent(&move);
    }

    if (!waitForLivePaint(app, bitmap, latestPosition)) {
        return 1;
    }

    if (!bitmap.liveStrokeActive()) {
        return 1;
    }

    if (bitmap.strokeCount() != 0) {
        return 1;
    }

    QMouseEvent release = mouseEvent(QEvent::MouseButtonRelease,
                                     latestPosition,
                                     Qt::LeftButton,
                                     Qt::NoButton);
    bitmap.mouseReleaseEvent(&release);

    if (!hasPaintNear(bitmap, latestPosition, 8)) {
        return 1;
    }

    if (!waitForCommittedPaint(app, bitmap, latestPosition)) {
        return 1;
    }

    if (bitmap.strokeCount() != 1) {
        return 1;
    }

    const int committedAlpha = alphaAt(bitmap, latestPosition);
    if (committedAlpha <= 0
            || !bitmap.undo()
            || bitmap.strokeCount() != 0
            || alphaAt(bitmap, latestPosition) != 0
            || !bitmap.redo()
            || bitmap.strokeCount() != 1
            || alphaAt(bitmap, latestPosition) <= 0) {
        return 1;
    }

    LivePreviewTestBitmap eraserBitmap;
    eraserBitmap.setWidth(160);
    eraserBitmap.setHeight(96);
    eraserBitmap.setBitmapDevicePixelRatio(1.0);
    if (!eraserBitmap.createFile(directory.filePath(QStringLiteral("eraser.png")), 160, 96)) {
        return 2;
    }
    eraserBitmap.setLivePreviewEnabled(true);
    eraserBitmap.setBrush(52.0, QColor{"#101318"}, 1.0, 1.0);
    eraserBitmap.setBrushSpacingRatio(0.0);

    QMouseEvent basePress = mouseEvent(QEvent::MouseButtonPress,
                                       QPointF{20.0, 48.0},
                                       Qt::LeftButton,
                                       Qt::LeftButton);
    eraserBitmap.mousePressEvent(&basePress);
    QMouseEvent baseMove = mouseEvent(QEvent::MouseMove,
                                      QPointF{140.0, 48.0},
                                      Qt::NoButton,
                                      Qt::LeftButton);
    eraserBitmap.mouseMoveEvent(&baseMove);
    QMouseEvent baseRelease = mouseEvent(QEvent::MouseButtonRelease,
                                         QPointF{140.0, 48.0},
                                         Qt::LeftButton,
                                         Qt::NoButton);
    eraserBitmap.mouseReleaseEvent(&baseRelease);

    const QPointF eraserPosition{80.0, 48.0};
    if (!waitForCommittedPaint(app, eraserBitmap, eraserPosition)
            || !waitForAlphaAtLeast(app, eraserBitmap, eraserPosition, 220)) {
        return 2;
    }

    eraserBitmap.setEraserMode(true);
    QMouseEvent eraserPress = mouseEvent(QEvent::MouseButtonPress,
                                         QPointF{60.0, 48.0},
                                         Qt::LeftButton,
                                         Qt::LeftButton);
    eraserBitmap.mousePressEvent(&eraserPress);
    for (int i = 1; i <= 20; ++i) {
        QMouseEvent eraserMove = mouseEvent(QEvent::MouseMove,
                                            QPointF{60.0 + static_cast<qreal>(i), 48.0},
                                            Qt::NoButton,
                                            Qt::LeftButton);
        eraserBitmap.mouseMoveEvent(&eraserMove);
    }

    if (!waitForAlphaAtMost(app, eraserBitmap, eraserPosition, 80)) {
        return 3;
    }

    if (!eraserBitmap.liveStrokeActive() || eraserBitmap.strokeCount() != 1) {
        return 4;
    }

    QMouseEvent eraserRelease = mouseEvent(QEvent::MouseButtonRelease,
                                           eraserPosition,
                                           Qt::LeftButton,
                                           Qt::NoButton);
    eraserBitmap.mouseReleaseEvent(&eraserRelease);
    if (!waitForCommittedAlphaAtMost(app, eraserBitmap, eraserPosition, 80, 2)) {
        return 5;
    }

    return 0;
}
