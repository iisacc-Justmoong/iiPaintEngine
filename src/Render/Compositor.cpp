//
// Created by Justmoong on 2026 May 24.
//

#include "Compositor.h"

#include <algorithm>
#include <array>
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

double dodge(double b, double s)
{
    return b == 0 ? 0 : (s == 1 ? 1 : std::min(1.0, b / (1 - s)));
}

double burn(double b, double s)
{
    return b == 1 ? 1 : (s == 0 ? 0 : 1 - std::min(1.0, (1 - b) / s));
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
    case RasterBlendMode::Darken: return std::min(source, destination);
    case RasterBlendMode::Lighten: return std::max(source, destination);
    case RasterBlendMode::ColorBurn: return burn(destination, source);
    case RasterBlendMode::ColorDodge: return dodge(destination, source);
    case RasterBlendMode::LinearBurn: return std::max(0.0, source + destination - 1);
    case RasterBlendMode::LinearDodge: return std::min(1.0, source + destination);
    case RasterBlendMode::HardLight: return overlayChannel(destination, source);
    case RasterBlendMode::SoftLight: {
        if (source <= 0.5)
            return destination - (1 - 2 * source) * destination * (1 - destination);
        const double d = destination <= 0.25
            ? ((16 * destination - 12) * destination + 4) * destination
            : std::sqrt(destination);
        return destination + (2 * source - 1) * (d - destination);
    }
    case RasterBlendMode::VividLight:
        return source <= 0.5 ? burn(destination, 2 * source) : dodge(destination, 2 * source - 1);
    case RasterBlendMode::LinearLight: return std::clamp(destination + 2 * source - 1, 0.0, 1.0);
    case RasterBlendMode::PinLight:
        return source <= 0.5 ? std::min(destination, 2 * source) : std::max(destination, 2 * source - 1);
    case RasterBlendMode::HardMix:
        return unsigned(byteFromUnit(source)) + unsigned(byteFromUnit(destination)) >= 255 ? 1 : 0;
    case RasterBlendMode::Difference: return std::abs(destination - source);
    case RasterBlendMode::Exclusion: return source + destination - 2 * source * destination;
    case RasterBlendMode::Subtract: return std::max(0.0, destination - source);
    case RasterBlendMode::Divide: return source == 0 ? 1 : std::min(1.0, destination / source);
    case RasterBlendMode::SourceOver:
    default:
        return source;
    }
}

using Color = std::array<double, 3>;
double luminance(const Color &c) { return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]; }
double saturation(const Color &c) { return *std::max_element(c.begin(), c.end()) - *std::min_element(c.begin(), c.end()); }

Color setLuminance(Color c, double target)
{
    const double delta = target - luminance(c);
    for (auto &v : c) v += delta;
    const double low = *std::min_element(c.begin(), c.end());
    const double high = *std::max_element(c.begin(), c.end());
    if (low < 0) for (auto &v : c) v = target + (v - target) * target / (target - low);
    if (high > 1) for (auto &v : c) v = target + (v - target) * (1 - target) / (high - target);
    return c;
}

Color setSaturation(Color c, double target)
{
    std::array<std::size_t, 3> order{0, 1, 2};
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return c[a] < c[b]; });
    const auto lo = order[0], mid = order[1], hi = order[2];
    c[mid] = c[hi] > c[lo] ? (c[mid] - c[lo]) * target / (c[hi] - c[lo]) : 0;
    c[hi] = c[hi] > c[lo] ? target : 0;
    c[lo] = 0;
    return c;
}

Color blendColor(Color s, Color b, RasterBlendMode mode)
{
    switch (mode) {
    case RasterBlendMode::Hue: return setLuminance(setSaturation(s, saturation(b)), luminance(b));
    case RasterBlendMode::Saturation: return setLuminance(setSaturation(b, saturation(s)), luminance(b));
    case RasterBlendMode::Color: return setLuminance(s, luminance(b));
    case RasterBlendMode::Luminosity: return setLuminance(b, luminance(s));
    case RasterBlendMode::DarkerColor: return s[0] + s[1] + s[2] < b[0] + b[1] + b[2] ? s : b;
    case RasterBlendMode::LighterColor: return s[0] + s[1] + s[2] > b[0] + b[1] + b[2] ? s : b;
    default: return {blendChannel(s[0], b[0], mode), blendChannel(s[1], b[1], mode), blendChannel(s[2], b[2], mode)};
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

    if (blendMode == RasterBlendMode::DestinationOut) {
        destination.alpha *= 1 - source.alpha;
        return destination.alpha > 0 ? destination : StraightPixel{};
    }
    const auto blended = blendColor({source.red, source.green, source.blue},
                                    {destination.red, destination.green, destination.blue}, blendMode);
    const double inverseSourceAlpha = 1.0 - source.alpha;
    const double outputAlpha = source.alpha + destination.alpha * inverseSourceAlpha;
    if (outputAlpha <= 0.0) {
        return {};
    }

    return StraightPixel{
            (source.alpha * ((1 - destination.alpha) * source.red + destination.alpha * blended[0])
                + destination.red * destination.alpha * inverseSourceAlpha) / outputAlpha,
            (source.alpha * ((1 - destination.alpha) * source.green + destination.alpha * blended[1])
                + destination.green * destination.alpha * inverseSourceAlpha) / outputAlpha,
            (source.alpha * ((1 - destination.alpha) * source.blue + destination.alpha * blended[2])
                + destination.blue * destination.alpha * inverseSourceAlpha) / outputAlpha,
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

            const StraightPixel destinationPixel = straightPixel(destination.pixels[index]);
            double opacity = std::clamp(layer.metadata.opacity, 0.0, 1.0)
                * layerMaskAlphaAt(layer.mask, x, y);
            if (layer.metadata.clipsToBelow) {
                opacity *= destinationPixel.alpha;
            }

            destination.pixels[index] = compositeRasterPixel(content.pixels[sourceIndex],
                destination.pixels[index], layer.metadata.blendMode, opacity,
                static_cast<std::uint32_t>(x) * 0x9e3779b9U ^ static_cast<std::uint32_t>(y));
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

std::uint32_t compositeRasterPixel(std::uint32_t source, std::uint32_t backdrop,
                                  RasterBlendMode mode, double opacity, std::uint32_t noiseKey)
{
    auto s = straightPixel(source);
    s.alpha *= std::isfinite(opacity) ? std::clamp(opacity, 0.0, 1.0) : 0;
    if (mode == RasterBlendMode::Dissolve) {
        noiseKey ^= noiseKey >> 16; noiseKey *= 0x7feb352dU;
        noiseKey ^= noiseKey >> 15; noiseKey *= 0x846ca68bU; noiseKey ^= noiseKey >> 16;
        s.alpha = double(noiseKey) / 4294967296.0 < s.alpha ? 1 : 0;
    }
    auto result = compositePixel(s, straightPixel(backdrop), mode);
    if (byteFromUnit(result.alpha) == 0) return 0;
    return argbFromStraight(result);
}

RasterLayer compositeLayerStack(const LayerStack &layers,
                                Types::Pixel width,
                                Types::Pixel height,
                                std::uint32_t clearArgb)
{
    RasterLayer result = makeRasterLayer(width, height, clearArgb);
    for (std::size_t index = 0; index < layers.layers.size();) {
        const Layer &base = layers.layers[index];
        if (base.metadata.clipsToBelow) { ++index; continue; }
        std::size_t end = index + 1;
        while (end < layers.layers.size() && layers.layers[end].metadata.clipsToBelow) ++end;
        if (end == index + 1) {
            compositeLayerOnto(result, base);
        } else if (base.metadata.visible) {
            Layer unscaled = base;
            unscaled.metadata.opacity = 1;
            unscaled.metadata.blendMode = RasterBlendMode::SourceOver;
            auto group = makeRasterLayer(width, height, 0);
            compositeLayerOnto(group, unscaled);
            for (std::size_t child = index + 1; child < end; ++child) {
                const auto &layer = layers.layers[child];
                if (!layer.metadata.visible) continue;
                const auto content = layerContent(layer, width, height);
                for (Types::Pixel y = 0; y < height; ++y) for (Types::Pixel x = 0; x < width; ++x) {
                    const auto p = pixelIndex(width, x, y);
                    if ((group.pixels[p] >> 24) == 0) continue;
                    const auto blended = compositeRasterPixel(content.pixels[p], group.pixels[p] | 0xff000000U,
                        layer.metadata.blendMode, layer.metadata.opacity * layerMaskAlphaAt(layer.mask, x, y),
                        static_cast<std::uint32_t>(x) * 0x9e3779b9U ^ static_cast<std::uint32_t>(y));
                    group.pixels[p] = (group.pixels[p] & 0xff000000U) | (blended & 0x00ffffffU);
                }
            }
            Layer flattened;
            flattened.surface = drawingSurfaceFromRasterLayer(std::move(group));
            flattened.metadata.opacity = base.metadata.opacity;
            flattened.metadata.blendMode = base.metadata.blendMode;
            compositeLayerOnto(result, flattened);
        }
        index = end;
    }
    return result;
}
