//
// Created by Justmoong on 2026 May 24.
//

#include "ColorSpace.h"

namespace {

bool chromaticityEquals(ColorChromaticity lhs, ColorChromaticity rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

} // namespace

ColorSpace makeSrgbColorSpace()
{
    return {};
}

ColorSpace makeDisplayP3LinearFloatColorSpace()
{
    ColorSpace colorSpace;
    colorSpace.name = "Display P3 Linear Float";
    colorSpace.primaries = ColorPrimaries::DisplayP3;
    colorSpace.transferFunction = ColorTransferFunction::Linear;
    colorSpace.componentEncoding = ColorComponentEncoding::Float32;
    colorSpace.linear = true;
    colorSpace.hdr = true;
    colorSpace.maxComponentValue = 16.0;
    colorSpace.referenceWhiteNits = 203.0;
    colorSpace.redPrimary = {0.680, 0.320};
    colorSpace.greenPrimary = {0.265, 0.690};
    colorSpace.bluePrimary = {0.150, 0.060};
    colorSpace.whitePoint = {0.3127, 0.3290};
    return colorSpace;
}

bool colorSpacesEquivalent(const ColorSpace &lhs, const ColorSpace &rhs)
{
    return lhs.name == rhs.name
            && lhs.primaries == rhs.primaries
            && lhs.transferFunction == rhs.transferFunction
            && lhs.componentEncoding == rhs.componentEncoding
            && lhs.iccProfile == rhs.iccProfile
            && lhs.linear == rhs.linear
            && lhs.hdr == rhs.hdr
            && lhs.minComponentValue == rhs.minComponentValue
            && lhs.maxComponentValue == rhs.maxComponentValue
            && lhs.referenceWhiteNits == rhs.referenceWhiteNits
            && chromaticityEquals(lhs.redPrimary, rhs.redPrimary)
            && chromaticityEquals(lhs.greenPrimary, rhs.greenPrimary)
            && chromaticityEquals(lhs.bluePrimary, rhs.bluePrimary)
            && chromaticityEquals(lhs.whitePoint, rhs.whitePoint);
}

bool colorSpaceSupportsWideGamut(const ColorSpace &colorSpace)
{
    return colorSpace.primaries == ColorPrimaries::DisplayP3
            || colorSpace.primaries == ColorPrimaries::Rec2020
            || colorSpace.primaries == ColorPrimaries::Custom;
}

bool colorSpaceSupportsHdr(const ColorSpace &colorSpace)
{
    return colorSpace.hdr || colorSpace.maxComponentValue > 1.0;
}

bool colorSpaceUsesFloatingPoint(const ColorSpace &colorSpace)
{
    return colorSpace.componentEncoding == ColorComponentEncoding::Float16
            || colorSpace.componentEncoding == ColorComponentEncoding::Float32;
}

int colorComponentBitDepth(const ColorSpace &colorSpace)
{
    switch (colorSpace.componentEncoding) {
    case ColorComponentEncoding::UInt16:
    case ColorComponentEncoding::Float16:
        return 16;
    case ColorComponentEncoding::Float32:
        return 32;
    case ColorComponentEncoding::UInt8:
    default:
        return 8;
    }
}
