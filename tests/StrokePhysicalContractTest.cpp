#include <cmath>
#include <vector>

#include "Stroke/Rasterizer.h"

namespace {

bool nearlyEqual(Types::Scalar lhs, Types::Scalar rhs)
{
    return std::abs(lhs - rhs) < 0.0001;
}

std::vector<BrushDab> streamedCornerDabs(const BrushState &brush)
{
    RasterDabStream stream{};
    std::vector<BrushDab> dabs = appendRasterDabs(
            stream,
            StrokePoint{{0.0, 0.0}, 0.25, 0.0, 0.0, 0.0, 0.0, 1},
            brush);
    std::vector<BrushDab> horizontal = appendRasterDabs(
            stream,
            StrokePoint{{5.0, 0.0}, 0.75, 1.0, 0.0, 0.0, 0.5, 1},
            brush);
    dabs.insert(dabs.end(), horizontal.begin(), horizontal.end());
    std::vector<BrushDab> vertical = appendRasterDabs(
            stream,
            StrokePoint{{5.0, 5.0}, 0.5, 2.0, 0.0, 1.0, 0.0, 1},
            brush,
            true);
    dabs.insert(dabs.end(), vertical.begin(), vertical.end());
    return dabs;
}

} // namespace

int main()
{
    BrushState brush{};
    brush.randomSeed = 17;
    brush.rasterizer.brushSize = 6.0;
    brush.rasterizer.spacingRatio = 0.5;
    brush.rasterizer.flow = 0.4;
    brush.rasterizer.rotationJitter = 0.25;

    BrushState untapered = brush;
    untapered.rasterizer.warmupDistance = 0.0;
    untapered.rasterizer.rotationJitter = 0.0;
    const std::vector<BrushDab> dabs = streamedCornerDabs(untapered);
    if (dabs.size() != 5
            || !nearlyEqual(dabs[0].position.x, 0.0)
            || !nearlyEqual(dabs[1].position.x, 3.0)
            || !nearlyEqual(dabs[2].position.x, 5.0)
            || !nearlyEqual(dabs[2].position.y, 1.0)
            || !nearlyEqual(dabs[3].position.y, 4.0)
            || !nearlyEqual(dabs[4].position.y, 5.0)) {
        return 1;
    }

    brush.rasterizer.warmupDistance = 3.0;
    const std::vector<BrushDab> seeded = streamedCornerDabs(brush);
    const std::vector<BrushDab> seededAgain = streamedCornerDabs(brush);
    BrushState differentSeed = brush;
    ++differentSeed.randomSeed;
    const std::vector<BrushDab> different = streamedCornerDabs(differentSeed);
    if (seeded.size() != different.size()
            || !nearlyEqual(seeded[1].rotationRadians, seededAgain[1].rotationRadians)
            || nearlyEqual(seeded[1].rotationRadians, different[1].rotationRadians)
            || !(seeded.front().alpha < seeded[1].alpha)) {
        return 1;
    }

    const DocumentRect bounds = documentBoundsForBrushDabs(seeded, brush.rasterizer);
    return !seeded.empty()
            && bounds.width > 0.0
            && bounds.height > 0.0
            && bounds.origin.x <= 0.0
            && bounds.origin.y <= 0.0
            ? 0
            : 1;
}
