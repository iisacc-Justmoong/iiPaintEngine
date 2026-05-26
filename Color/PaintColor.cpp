//
// Created by Justmoong on 2026 May 24.
//

#include "PaintColor.h"

#include <algorithm>

PaintColor makePaintColor(const ColorSpace &colorSpace,
                          Types::Scalar red,
                          Types::Scalar green,
                          Types::Scalar blue,
                          Types::Scalar alpha)
{
    PaintColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;
    color.alpha = alpha;
    color.colorSpace = colorSpace;
    color.hdr = colorSpaceSupportsHdr(colorSpace)
            && (red > 1.0 || green > 1.0 || blue > 1.0);
    return color;
}

PaintColor clampPaintColorToColorSpace(const PaintColor &color)
{
    PaintColor clamped = color;
    const Types::Scalar minValue = color.colorSpace.minComponentValue;
    const Types::Scalar maxValue = color.colorSpace.maxComponentValue;
    clamped.red = std::clamp(color.red, minValue, maxValue);
    clamped.green = std::clamp(color.green, minValue, maxValue);
    clamped.blue = std::clamp(color.blue, minValue, maxValue);
    clamped.alpha = std::clamp(color.alpha, 0.0, 1.0);
    clamped.hdr = colorSpaceSupportsHdr(color.colorSpace)
            && (clamped.red > 1.0 || clamped.green > 1.0 || clamped.blue > 1.0);
    return clamped;
}
