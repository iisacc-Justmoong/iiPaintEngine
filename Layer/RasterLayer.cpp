//
// Created by Justmoong on 2026 May 24.
//

#include "RasterLayer.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

double channelOf(std::uint32_t argb, unsigned shift)
{
    return static_cast<double>((argb >> shift) & 0xFFU) / 255.0;
}

std::uint8_t byteFromUnit(double value)
{
    return static_cast<std::uint8_t>(std::clamp<int>(
            static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0)),
            0,
            255));
}

PremultipliedPixel premultiply(std::uint32_t argb)
{
    const double alpha = static_cast<double>(alphaOf(argb)) / 255.0;
    return PremultipliedPixel{
            channelOf(argb, 16U) * alpha,
            channelOf(argb, 8U) * alpha,
            channelOf(argb, 0U) * alpha,
            alpha,
    };
}

std::uint32_t unpremultiply(PremultipliedPixel pixel)
{
    if (pixel.alpha <= 0.0) {
        return 0x00000000U;
    }

    const std::uint8_t alpha = byteFromUnit(pixel.alpha);
    const std::uint8_t red = byteFromUnit(pixel.red / pixel.alpha);
    const std::uint8_t green = byteFromUnit(pixel.green / pixel.alpha);
    const std::uint8_t blue = byteFromUnit(pixel.blue / pixel.alpha);
    return (static_cast<std::uint32_t>(alpha) << 24U)
            | (static_cast<std::uint32_t>(red) << 16U)
            | (static_cast<std::uint32_t>(green) << 8U)
            | static_cast<std::uint32_t>(blue);
}

PremultipliedPixel sourceOver(PremultipliedPixel source, PremultipliedPixel destination)
{
    const double inverseSourceAlpha = 1.0 - source.alpha;
    return PremultipliedPixel{
            source.red + destination.red * inverseSourceAlpha,
            source.green + destination.green * inverseSourceAlpha,
            source.blue + destination.blue * inverseSourceAlpha,
            source.alpha + destination.alpha * inverseSourceAlpha,
    };
}

PremultipliedPixel clampPremultipliedAlpha(PremultipliedPixel pixel, double alphaCap)
{
    const double clampedCap = std::clamp(alphaCap, 0.0, 1.0);
    if (pixel.alpha <= clampedCap || pixel.alpha <= 0.0) {
        return pixel;
    }

    const double scale = clampedCap / pixel.alpha;
    return PremultipliedPixel{
            pixel.red * scale,
            pixel.green * scale,
            pixel.blue * scale,
            clampedCap,
    };
}

bool contains(const RasterLayer &layer, DevicePixelPoint position)
{
    return position.x >= 0
            && position.y >= 0
            && position.x < layer.width
            && position.y < layer.height;
}

std::size_t pixelIndex(const RasterLayer &layer, DevicePixelPoint position)
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(layer.width)
            + static_cast<std::size_t>(position.x);
}

bool contains(const StrokeCompositeBuffer &buffer, DevicePixelPoint position)
{
    return position.x >= 0
            && position.y >= 0
            && position.x < buffer.width
            && position.y < buffer.height;
}

std::size_t pixelIndex(const StrokeCompositeBuffer &buffer, DevicePixelPoint position)
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(buffer.width)
            + static_cast<std::size_t>(position.x);
}

} // namespace

RasterLayer makeRasterLayer(Types::Pixel width, Types::Pixel height, std::uint32_t clearArgb)
{
    RasterLayer layer;
    layer.width = std::max<Types::Pixel>(0, width);
    layer.height = std::max<Types::Pixel>(0, height);
    layer.pixels.assign(static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height),
                        clearArgb);
    return layer;
}

StrokeCompositeBuffer makeStrokeCompositeBuffer(Types::Pixel width, Types::Pixel height)
{
    StrokeCompositeBuffer buffer;
    buffer.width = std::max<Types::Pixel>(0, width);
    buffer.height = std::max<Types::Pixel>(0, height);
    buffer.pixels.assign(static_cast<std::size_t>(buffer.width) * static_cast<std::size_t>(buffer.height),
                         PremultipliedPixel{});
    return buffer;
}

void accumulateStrokeSamples(StrokeCompositeBuffer &buffer, const std::vector<RasterSample> &samples)
{
    for (const RasterSample &sample : samples) {
        if (!contains(buffer, sample.position)) {
            continue;
        }

        const std::size_t index = pixelIndex(buffer, sample.position);
        const double alphaCap = static_cast<double>(sample.opacityCap) / 255.0;
        if (buffer.pixels[index].alpha >= alphaCap) {
            continue;
        }

        PremultipliedPixel blended = sourceOver(premultiply(sample.argb), buffer.pixels[index]);
        if (sample.blendMode == RasterBlendMode::SourceOver) {
            buffer.pixels[index] = clampPremultipliedAlpha(blended, alphaCap);
        }
    }
}

std::uint32_t strokeCompositePixelAt(const StrokeCompositeBuffer &buffer, DevicePixelPoint position)
{
    if (!contains(buffer, position)) {
        return 0x00000000U;
    }

    const std::size_t index = pixelIndex(buffer, position);
    if (index >= buffer.pixels.size()) {
        return 0x00000000U;
    }

    return unpremultiply(buffer.pixels[index]);
}

void compositeStrokeBufferOntoLayer(RasterLayer &layer, const StrokeCompositeBuffer &buffer)
{
    const Types::Pixel width = std::min(layer.width, buffer.width);
    const Types::Pixel height = std::min(layer.height, buffer.height);
    for (Types::Pixel y = 0; y < height; ++y) {
        for (Types::Pixel x = 0; x < width; ++x) {
            const DevicePixelPoint position{x, y};
            const std::size_t layerIndex = pixelIndex(layer, position);
            const std::size_t bufferIndex = pixelIndex(buffer, position);
            const PremultipliedPixel source = buffer.pixels[bufferIndex];
            if (source.alpha <= 0.0) {
                continue;
            }

            layer.pixels[layerIndex] = unpremultiply(sourceOver(source, premultiply(layer.pixels[layerIndex])));
        }
    }
}

void paintRasterSamples(RasterLayer &layer, const std::vector<RasterSample> &samples)
{
    StrokeCompositeBuffer buffer = makeStrokeCompositeBuffer(layer.width, layer.height);
    accumulateStrokeSamples(buffer, samples);
    compositeStrokeBufferOntoLayer(layer, buffer);
}

std::uint32_t rasterLayerPixelAt(const RasterLayer &layer, DevicePixelPoint position)
{
    if (!contains(layer, position)) {
        return 0x00000000U;
    }

    const std::size_t index = pixelIndex(layer, position);
    if (index >= layer.pixels.size()) {
        return 0x00000000U;
    }

    return layer.pixels[index];
}
