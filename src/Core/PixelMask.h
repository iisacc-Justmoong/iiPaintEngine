#pragma once
#include <cstdint>
#include <vector>
#include "Core/Types.h"

// An owned row-major pixel mask. Values are finite normalized coverage, not ARGB.
struct PixelMask {
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<float> coverage;
};

inline constexpr Types::Pixel maxPixelMaskDimension = 16384;
inline constexpr std::uint64_t maxPixelMaskPixelCount = 64ULL * 1024 * 1024;

PixelMask makePixelMask(Types::Pixel width, Types::Pixel height, float clearCoverage = 0);
bool isValidPixelMask(const PixelMask &mask);
