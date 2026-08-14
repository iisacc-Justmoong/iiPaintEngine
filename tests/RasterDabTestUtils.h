#pragma once

#include <vector>

#include "Stroke/Rasterizer.h"

inline std::vector<BrushDab> streamTestDabs(const BrushState &brush,
                                            const StrokePoint &first,
                                            const StrokePoint &last)
{
    RasterDabStream stream{};
    std::vector<BrushDab> dabs = appendRasterDabs(stream, first, brush);
    std::vector<BrushDab> finalDabs = appendRasterDabs(stream, last, brush, true);
    dabs.insert(dabs.end(), finalDabs.begin(), finalDabs.end());
    return dabs;
}

inline std::vector<BrushDab> streamTestDabs(const BrushState &brush,
                                            const StrokePoint &first,
                                            const StrokePoint &middle,
                                            const StrokePoint &last)
{
    RasterDabStream stream{};
    std::vector<BrushDab> dabs = appendRasterDabs(stream, first, brush);
    std::vector<BrushDab> middleDabs = appendRasterDabs(stream, middle, brush);
    dabs.insert(dabs.end(), middleDabs.begin(), middleDabs.end());
    std::vector<BrushDab> finalDabs = appendRasterDabs(stream, last, brush, true);
    dabs.insert(dabs.end(), finalDabs.begin(), finalDabs.end());
    return dabs;
}
