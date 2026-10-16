#include <iiPaintEngine>
#include <cmath>
#include <iostream>
int main(int argc,char **argv)
{
    if(argc!=2) return 1;
    constexpr int width=720,rowHeight=106;
    auto presets=builtInBrushPresets();
    auto gallery=makeRasterLayer(width,rowHeight*static_cast<int>(presets.size()),0xFFF4F2EEU);
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
        const int top=static_cast<int>(row)*rowHeight;
        auto rowPixels=makeRasterLayer(panelWidth,panelHeight,0xFFFFFFFFU);
        std::vector<RasterSample> samples;
        for(int y=0;y<panelHeight;++y) for(int x=0;x<panelWidth;++x)
            samples.push_back({{x,y},rasterLayerPixelAt(layer,{x,y}),0xFFU});
        paintRasterSamples(rowPixels,samples);
        for(int y=0;y<panelHeight;++y) for(int x=0;x<panelWidth;++x)
            gallery.pixels[(top+y)*width+x+15]=rasterLayerPixelAt(rowPixels,{x,y});
    }
    BitmapFile file;
    if(!file.create(argv[1],gallery.width,gallery.height,"png")) return 3;
    if(!file.replacePixels(gallery)||!file.save()) return 4;
    std::cout<<"Rendered "<<presets.size()<<" brush presets\n";
    return 0;
}
