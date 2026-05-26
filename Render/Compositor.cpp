//
// Created by Justmoong on 2026 May 24.
//

#include "Compositor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

struct StraightPixel {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;
};

std::uint8_t alphaByte(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

double alphaOf(std::uint32_t argb)
{
    return static_cast<double>(alphaByte(argb)) / 255.0;
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

StraightPixel straightPixel(std::uint32_t argb)
{
    return StraightPixel{
            channelOf(argb, 16U),
            channelOf(argb, 8U),
            channelOf(argb, 0U),
            alphaOf(argb),
    };
}

std::uint32_t argbFromStraight(StraightPixel pixel)
{
    const std::uint8_t alpha = byteFromUnit(pixel.alpha);
    const std::uint8_t red = byteFromUnit(pixel.red);
    const std::uint8_t green = byteFromUnit(pixel.green);
    const std::uint8_t blue = byteFromUnit(pixel.blue);
    return (static_cast<std::uint32_t>(alpha) << 24U)
            | (static_cast<std::uint32_t>(red) << 16U)
            | (static_cast<std::uint32_t>(green) << 8U)
            | static_cast<std::uint32_t>(blue);
}

std::size_t pixelIndex(Types::Pixel width, Types::Pixel x, Types::Pixel y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
}

double layerMaskAlphaAt(const LayerMask &mask, Types::Pixel x, Types::Pixel y)
{
    if (!mask.enabled) {
        return 1.0;
    }
    if (x < 0 || y < 0 || x >= mask.width || y >= mask.height) {
        return 0.0;
    }

    const std::size_t index = pixelIndex(mask.width, x, y);
    if (index >= mask.alpha.size()) {
        return 0.0;
    }
    return static_cast<double>(mask.alpha[index]) / 255.0;
}

double overlayChannel(double source, double destination)
{
    if (destination <= 0.5) {
        return 2.0 * source * destination;
    }
    return 1.0 - 2.0 * (1.0 - source) * (1.0 - destination);
}

double blendChannel(double source, double destination, RasterBlendMode blendMode)
{
    switch (blendMode) {
    case RasterBlendMode::Multiply:
        return source * destination;
    case RasterBlendMode::Screen:
        return 1.0 - (1.0 - source) * (1.0 - destination);
    case RasterBlendMode::Overlay:
        return overlayChannel(source, destination);
    case RasterBlendMode::SourceOver:
    default:
        return source;
    }
}

StraightPixel compositePixel(StraightPixel source,
                             StraightPixel destination,
                             RasterBlendMode blendMode)
{
    source.alpha = std::clamp(source.alpha, 0.0, 1.0);
    destination.alpha = std::clamp(destination.alpha, 0.0, 1.0);
    if (source.alpha <= 0.0) {
        return destination;
    }

    const double blendedRed = blendChannel(source.red, destination.red, blendMode);
    const double blendedGreen = blendChannel(source.green, destination.green, blendMode);
    const double blendedBlue = blendChannel(source.blue, destination.blue, blendMode);
    const double inverseSourceAlpha = 1.0 - source.alpha;
    const double outputAlpha = source.alpha + destination.alpha * inverseSourceAlpha;
    if (outputAlpha <= 0.0) {
        return {};
    }

    return StraightPixel{
            (blendedRed * source.alpha + destination.red * destination.alpha * inverseSourceAlpha) / outputAlpha,
            (blendedGreen * source.alpha + destination.green * destination.alpha * inverseSourceAlpha) / outputAlpha,
            (blendedBlue * source.alpha + destination.blue * destination.alpha * inverseSourceAlpha) / outputAlpha,
            outputAlpha,
    };
}

RasterLayer layerContent(const Layer &layer, Types::Pixel width, Types::Pixel height);

void compositeLayerOnto(RasterLayer &destination, const Layer &layer)
{
    if (!layer.metadata.visible) {
        return;
    }

    const RasterLayer content = layerContent(layer, destination.width, destination.height);
    const Types::Pixel width = std::min(destination.width, content.width);
    const Types::Pixel height = std::min(destination.height, content.height);
    for (Types::Pixel y = 0; y < height; ++y) {
        for (Types::Pixel x = 0; x < width; ++x) {
            const std::size_t index = pixelIndex(destination.width, x, y);
            if (index >= destination.pixels.size()) {
                continue;
            }

            const std::size_t sourceIndex = pixelIndex(content.width, x, y);
            if (sourceIndex >= content.pixels.size()) {
                continue;
            }

            StraightPixel source = straightPixel(content.pixels[sourceIndex]);
            const StraightPixel destinationPixel = straightPixel(destination.pixels[index]);
            source.alpha *= std::clamp(layer.metadata.opacity, 0.0, 1.0);
            source.alpha *= layerMaskAlphaAt(layer.mask, x, y);
            if (layer.metadata.clipsToBelow) {
                source.alpha *= destinationPixel.alpha;
            }

            destination.pixels[index] = argbFromStraight(compositePixel(source,
                                                                        destinationPixel,
                                                                        layer.metadata.blendMode));
        }
    }
}

RasterLayer layerContent(const Layer &layer, Types::Pixel width, Types::Pixel height)
{
    RasterLayer content = makeRasterLayer(width, height, 0x00000000U);
    const Types::Pixel copyWidth = std::min<Types::Pixel>(width, layer.surface.width);
    const Types::Pixel copyHeight = std::min<Types::Pixel>(height, layer.surface.height);
    for (Types::Pixel y = 0; y < copyHeight; ++y) {
        for (Types::Pixel x = 0; x < copyWidth; ++x) {
            const std::size_t sourceIndex = pixelIndex(layer.surface.width, x, y);
            const std::size_t destinationIndex = pixelIndex(content.width, x, y);
            if (sourceIndex < layer.surface.pixels.size() && destinationIndex < content.pixels.size()) {
                content.pixels[destinationIndex] = layer.surface.pixels[sourceIndex];
            }
        }
    }

    if (!layer.children.empty()) {
        LayerStack childStack;
        childStack.layers = layer.children;
        const RasterLayer children = compositeLayerStack(childStack, width, height);
        Layer childCompositeLayer;
        childCompositeLayer.surface = drawingSurfaceFromRasterLayer(children);
        compositeLayerOnto(content, childCompositeLayer);
    }
    return content;
}

} // namespace

RasterLayer compositeLayerStack(const LayerStack &layers,
                                Types::Pixel width,
                                Types::Pixel height,
                                std::uint32_t clearArgb)
{
    RasterLayer result = makeRasterLayer(width, height, clearArgb);
    for (const Layer &layer : layers.layers) {
        compositeLayerOnto(result, layer);
    }
    return result;
}
