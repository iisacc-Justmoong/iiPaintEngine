#include <cstddef>

#include "Color/ColorSpace.h"
#include "Color/PaintColor.h"

namespace {

bool byteVectorEquals(const std::vector<std::byte> &bytes, std::initializer_list<unsigned int> expected)
{
    if (bytes.size() != expected.size()) {
        return false;
    }

    std::size_t index = 0;
    for (const unsigned int expectedByte : expected) {
        if (std::to_integer<unsigned int>(bytes[index]) != expectedByte) {
            return false;
        }
        ++index;
    }
    return true;
}

} // namespace

int main()
{
    static_assert(ColorPrimaries::Srgb != ColorPrimaries::DisplayP3);
    static_assert(ColorTransferFunction::Srgb != ColorTransferFunction::Linear);
    static_assert(ColorComponentEncoding::UInt8 != ColorComponentEncoding::Float32);

    const ColorSpace srgb = makeSrgbColorSpace();
    if (srgb.name != "sRGB"
            || srgb.primaries != ColorPrimaries::Srgb
            || srgb.transferFunction != ColorTransferFunction::Srgb
            || srgb.componentEncoding != ColorComponentEncoding::UInt8
            || srgb.linear
            || srgb.hdr
            || colorComponentBitDepth(srgb) != 8
            || colorSpaceUsesFloatingPoint(srgb)
            || colorSpaceSupportsWideGamut(srgb)
            || colorSpaceSupportsHdr(srgb)) {
        return 1;
    }

    ColorSpace displayP3 = makeDisplayP3LinearFloatColorSpace();
    displayP3.iccProfile = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    if (displayP3.name != "Display P3 Linear Float"
            || displayP3.primaries != ColorPrimaries::DisplayP3
            || displayP3.transferFunction != ColorTransferFunction::Linear
            || displayP3.componentEncoding != ColorComponentEncoding::Float32
            || !displayP3.linear
            || !displayP3.hdr
            || displayP3.maxComponentValue <= 1.0
            || colorComponentBitDepth(displayP3) != 32
            || !colorSpaceUsesFloatingPoint(displayP3)
            || !colorSpaceSupportsWideGamut(displayP3)
            || !colorSpaceSupportsHdr(displayP3)
            || !byteVectorEquals(displayP3.iccProfile, {0x01, 0x02, 0x03})) {
        return 1;
    }

    ColorSpace displayP3Copy = displayP3;
    if (!colorSpacesEquivalent(displayP3, displayP3Copy)) {
        return 1;
    }
    displayP3Copy.referenceWhiteNits = 160.0;
    if (colorSpacesEquivalent(displayP3, displayP3Copy)) {
        return 1;
    }

    PaintColor hdrRed = makePaintColor(displayP3, 2.5, 0.25, 0.0, 1.0);
    if (!hdrRed.hdr
            || hdrRed.colorSpace.componentEncoding != ColorComponentEncoding::Float32
            || hdrRed.red != 2.5
            || hdrRed.alpha != 1.0) {
        return 1;
    }

    PaintColor clippedSrgb = clampPaintColorToColorSpace(makePaintColor(srgb, 1.5, -0.5, 0.25, 1.2));
    if (clippedSrgb.red != 1.0
            || clippedSrgb.green != 0.0
            || clippedSrgb.blue != 0.25
            || clippedSrgb.alpha != 1.0
            || clippedSrgb.hdr) {
        return 1;
    }

    return 0;
}
