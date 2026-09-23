#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
#include <limits>
#include "QtAdapter/BitmapFileItem.h"
#include "Brush/BrushLibrary.h"
#include "Brush/BrushPresetSerializer.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
class TestBitmap : public BitmapFileItem {
public:
    using BitmapFileItem::mousePressEvent;
    using BitmapFileItem::mouseReleaseEvent;
    using BitmapFileItem::mouseMoveEvent;
};
void sendMouse(TestBitmap &item, QEvent::Type type, double x=32, double y=32)
{
    QMouseEvent event{type,{x,y},{x,y},{x,y},type==QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                      type==QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,Qt::NoModifier};
    if (type==QEvent::MouseButtonPress) item.mousePressEvent(&event);
    else if (type==QEvent::MouseButtonRelease) item.mouseReleaseEvent(&event);
    else item.mouseMoveEvent(&event);
}
QImage render(TestBitmap &item)
{
    QImage image(64,64,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    QPainter painter(&image); item.paint(&painter); painter.end(); return image;
}
void pump(int milliseconds)
{
    QEventLoop loop; QTimer::singleShot(milliseconds,&loop,&QEventLoop::quit); loop.exec();
}
bool samePixels(QImage lhs, QImage rhs)
{
    lhs=lhs.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    rhs=rhs.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (lhs.size()!=rhs.size()) return false;
    for (int y=0;y<lhs.height();++y) for (int x=0;x<lhs.width();++x) {
        if (lhs.pixel(x,y)!=rhs.pixel(x,y)) {
            std::cerr << "Pixel mismatch " << x << ',' << y << ' ' << std::hex
                      << lhs.pixel(x,y) << ' ' << rhs.pixel(x,y) << std::dec << '\n';
            return false;
        }
    }
    return true;
}
int main(int argc,char **argv)
{
    QGuiApplication app(argc,argv);
    QTemporaryDir directory(QStringLiteral(IIPAINTENGINE_BINARY_DIR "/advanced-brush-ui-XXXXXX"));
    CHECK(directory.isValid());
    TestBitmap item; item.setWidth(64); item.setHeight(64);
    CHECK(item.createFile(directory.filePath("brush.png"),64,64,"png"));
    auto preset=builtInBrushPresets()[2]; // Continuous soft airbrush.
    preset.size=20; preset.flow=.1F; preset.stroke.airbrushRate=100;
    const auto data=QString::fromStdString(serializeBrushPreset(preset));
    CHECK(item.setBrushPresetData(data));
    CHECK(item.brushSize()==20 && item.lastBrushError().isEmpty());
    const auto before=item.brushPresetData();
    CHECK(!item.setBrushPresetData(QStringLiteral("invalid")) && !item.lastBrushError().isEmpty());
    CHECK(item.brushPresetData()==before);
    item.setBrushSize(24); item.setBrushFlow(.12);
    const auto changed=readBrushPreset(item.brushPresetData().toStdString());
    CHECK(changed.preset && changed.preset->size==24 && changed.preset->stroke.airbrushEnabled);
    item.setBrushColor(Qt::red);
    sendMouse(item,QEvent::MouseButtonPress);
    CHECK(item.liveStrokeActive());
    const auto initial=render(item);
    const auto initialAlpha=qAlpha(initial.pixel(32,32));
    CHECK(initialAlpha>0 && initialAlpha<100);
    pump(100);
    const auto live=render(item);
    CHECK(qAlpha(live.pixel(32,32))>initialAlpha+20);
    // A preset change applies only to the next stroke; current airbrush keeps its state.
    auto marker=builtInBrushPresets()[3]; marker.size=4;
    CHECK(item.setBrushPreset(marker));
    sendMouse(item,QEvent::MouseButtonRelease);
    CHECK(!item.liveStrokeActive() && item.strokeCount()==1);
    const auto committed=item.bitmapImage();
    pump(40);
    CHECK(item.bitmapImage()==committed);
    CHECK(samePixels(render(item),committed));
    CHECK(item.undo() && qAlpha(item.bitmapImage().pixel(32,32))==0);
    CHECK(item.redo() && item.bitmapImage()==committed);
    CHECK(item.clear());
    sendMouse(item,QEvent::MouseButtonPress);
    sendMouse(item,QEvent::MouseButtonRelease);
    const auto markerImage=item.bitmapImage();
    CHECK(qAlpha(markerImage.pixel(32,32))>0 && qAlpha(markerImage.pixel(40,32))==0);
    CHECK(item.clear());
    CHECK(item.setBrushPreset(preset));
    sendMouse(item,QEvent::MouseButtonPress);
    item.setZoom(2); // Cancels and discards pending pixels, including timed emission.
    CHECK(!item.liveStrokeActive());
    pump(40);
    CHECK(qAlpha(item.bitmapImage().pixel(32,32))==0);
    item.resetBrushPreset(); CHECK(item.brushPresetData().isEmpty());
    return 0;
}
