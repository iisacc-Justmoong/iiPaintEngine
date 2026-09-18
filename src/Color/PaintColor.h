//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Color/ColorSpace.h"
#include "Core/Types.h"

struct PaintColor {
    Types::Scalar red = 0.0;
    Types::Scalar green = 0.0;
    Types::Scalar blue = 0.0;
    Types::Scalar alpha = 1.0;
    ColorSpace colorSpace;
    bool premultiplied = false;
    bool hdr = false;
};

PaintColor makePaintColor(const ColorSpace &colorSpace,
                          Types::Scalar red,
                          Types::Scalar green,
                          Types::Scalar blue,
                          Types::Scalar alpha = 1.0);

PaintColor clampPaintColorToColorSpace(const PaintColor &color);
