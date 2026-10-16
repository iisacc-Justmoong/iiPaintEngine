#include "PixelMask.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
bool validDimensions(Types::Pixel width, Types::Pixel height)
{
    return width > 0 && height > 0 && width <= maxPixelMaskDimension && height <= maxPixelMaskDimension
            && static_cast<std::uint64_t>(width) * height <= maxPixelMaskPixelCount;
}
}
PixelMask makePixelMask(Types::Pixel width, Types::Pixel height, float clearCoverage)
{
    if (!validDimensions(width,height) || !std::isfinite(clearCoverage) || clearCoverage < 0 || clearCoverage > 1)
        throw std::invalid_argument("Invalid pixel mask dimensions or normalized coverage.");
    return {width,height,std::vector<float>(static_cast<std::size_t>(width) * height,clearCoverage)};
}
bool isValidPixelMask(const PixelMask &mask)
{
    return validDimensions(mask.width,mask.height)
            && mask.coverage.size() == static_cast<std::size_t>(mask.width) * mask.height
            && std::ranges::all_of(mask.coverage,[](float value) {return std::isfinite(value) && value >= 0 && value <= 1;});
}
