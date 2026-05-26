//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Core/Types.h"

enum class ColorPrimaries {
    Srgb,
    DisplayP3,
    Rec2020,
    Custom,
};

enum class ColorTransferFunction {
    Srgb,
    Linear,
    Gamma22,
    Pq,
    Hlg,
};

enum class ColorComponentEncoding {
    UInt8,
    UInt16,
    Float16,
    Float32,
};

struct ColorChromaticity {
    Types::Scalar x = 0.0;
    Types::Scalar y = 0.0;
};

struct ColorSpace {
    std::string name = "sRGB";
    ColorPrimaries primaries = ColorPrimaries::Srgb;
    ColorTransferFunction transferFunction = ColorTransferFunction::Srgb;
    ColorComponentEncoding componentEncoding = ColorComponentEncoding::UInt8;
    std::vector<std::byte> iccProfile;
    bool linear = false;
    bool hdr = false;
    Types::Scalar minComponentValue = 0.0;
    Types::Scalar maxComponentValue = 1.0;
    Types::Scalar referenceWhiteNits = 80.0;
    ColorChromaticity redPrimary{0.64, 0.33};
    ColorChromaticity greenPrimary{0.30, 0.60};
    ColorChromaticity bluePrimary{0.15, 0.06};
    ColorChromaticity whitePoint{0.3127, 0.3290};
};

ColorSpace makeSrgbColorSpace();

ColorSpace makeDisplayP3LinearFloatColorSpace();

bool colorSpacesEquivalent(const ColorSpace &lhs, const ColorSpace &rhs);

bool colorSpaceSupportsWideGamut(const ColorSpace &colorSpace);

bool colorSpaceSupportsHdr(const ColorSpace &colorSpace);

bool colorSpaceUsesFloatingPoint(const ColorSpace &colorSpace);

int colorComponentBitDepth(const ColorSpace &colorSpace);
