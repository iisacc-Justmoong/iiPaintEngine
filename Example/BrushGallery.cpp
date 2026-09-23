#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <cmath>
#include <iostream>
#include "Brush/BrushLibrary.h"
#include "Brush/BrushResolve.h"
#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

// A headless, reproducible consumer of the same immediate-pixel API as BitmapFileItem.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2) { std::cerr << "Usage: iiPaintEngineBrushGallery output.png\n"; return 1; }
    constexpr int width = 960, rowHeight = 106;
    auto presets = builtInBrushPresets();
    QImage gallery(width, 84 + rowHeight * static_cast<int>(presets.size()), QImage::Format_ARGB32);
    gallery.fill(QColor("#F4F2EE"));
    QPainter painter(&gallery);
    painter.setPen(QColor("#202833"));
    painter.setFont(QFont("Helvetica", 22, QFont::Bold));
    painter.drawText(QPoint(28, 36), "iiPaintEngine / brush studies");
    painter.setFont(QFont("Helvetica", 11));
    painter.drawText(QPoint(28, 60), "Native raster output  |  pressure, tilt, time and seeded variation");
    for (std::size_t row = 0; row < presets.size(); ++row) {
        auto &preset = presets[row];
        if (row == 1) preset.size = 12;
        auto resolved = resolveBrushPreset(preset, row == 4 ? 0xFF208C68U : 0xFF3449ADU, 917);
        if (!resolved.brush) return 2;
        auto &brush = *resolved.brush;
        constexpr int panelWidth = 690, panelHeight = 90;
        auto layer = makeRasterLayer(panelWidth, panelHeight);
        if (row == 5) {
            for (int y = 25; y < 65; ++y)
                for (int x = 200; x < 370; ++x) layer.pixels[y * panelWidth + x] = 0xFFD27258U;
        }
        auto pixels = makeStrokeCompositeBuffer(panelWidth, panelHeight);
        RasterDabStream stream;
        RasterSourceSampler source;
        source.context = &layer; source.width = panelWidth; source.height = panelHeight;
        source.sampleArgb = [](const void *context, DevicePixelPoint point) {
            return rasterLayerPixelAt(*static_cast<const RasterLayer *>(context), point);
        };
        // Events are immediately rasterized; only the preceding point is retained by the engine.
        for (int step = 0; step <= 160; ++step) {
            const double t = step / 160.0;
            StrokePoint point{{40 + t * 600, 45 + 15 * std::sin(t * 9)}, 0.15 + 0.85 * std::sin(t * 3.141592653589793), t * 2};
            point.tiltX = 0.3 * std::sin(t * 6); point.tiltY = 0.2;
            point.rotationRadians = t;
            const auto dabs = appendRasterDabs(stream, point, brush, step == 160);
            const auto samples = projectBrushDabs(dabs, brush.rasterizer, {}, source, brush.material);
            accumulateStrokeSamples(pixels, samples);
        }
        compositeStrokeBufferOntoLayer(layer, pixels);
        QImage panel(reinterpret_cast<const uchar *>(layer.pixels.data()), panelWidth, panelHeight,
                     panelWidth * 4, QImage::Format_ARGB32);
        const int top = 84 + static_cast<int>(row) * rowHeight;
        painter.fillRect(18, top, width - 36, rowHeight - 10, Qt::white);
        painter.setPen(QColor("#202833"));
        painter.setFont(QFont("Helvetica", 13, QFont::Bold));
        painter.drawText(QPoint(34, top + 39), QString::fromStdString(preset.name));
        painter.setFont(QFont("Helvetica", 10));
        painter.setPen(QColor("#6D7480"));
        painter.drawText(QPoint(34, top + 61), QString("%1 px  /  flow %2").arg(preset.size).arg(preset.flow, 0, 'f', 2));
        painter.drawImage(240, top + 3, panel);
    }
    painter.end();
    return gallery.save(QString::fromLocal8Bit(argv[1]), "PNG") ? 0 : 3;
}
