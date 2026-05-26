//
// Created by Justmoong on 2026 May 24.
//

#include "RenderContext.h"

bool renderColorSpacesMatch(const ColorSpace &source, const ColorSpace &target)
{
    return colorSpacesEquivalent(source, target);
}

RenderBufferFormat renderBufferFormatForColorSpace(const ColorSpace &colorSpace)
{
    switch (colorSpace.componentEncoding) {
    case ColorComponentEncoding::UInt16:
        return RenderBufferFormat::UInt16;
    case ColorComponentEncoding::Float16:
        return RenderBufferFormat::Float16;
    case ColorComponentEncoding::Float32:
        return RenderBufferFormat::Float32;
    case ColorComponentEncoding::UInt8:
    default:
        return RenderBufferFormat::UInt8;
    }
}

bool renderColorSpaceRequiresLinearCompositing(const ColorSpace &colorSpace)
{
    return colorSpace.linear
            || colorSpace.transferFunction == ColorTransferFunction::Linear
            || colorSpaceSupportsHdr(colorSpace)
            || colorSpaceUsesFloatingPoint(colorSpace);
}
