#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << ": " #condition "\n"; return 1; \
} } while (false)

namespace {
std::uint8_t alpha(std::uint32_t argb)
{
    return static_cast<std::uint8_t>(argb >> 24U);
}

const RasterSample *at(const std::vector<RasterSample> &samples, DevicePixelPoint point)
{
    const auto it = std::find_if(samples.begin(), samples.end(), [&](const RasterSample &sample) {
        return sample.position.x == point.x && sample.position.y == point.y;
    });
    return it == samples.end() ? nullptr : &*it;
}

bool checkTip(Rasterizer rasterizer, DevicePixelPoint point)
{
    rasterizer.opacity = .5;
    BrushDab dab;
    dab.position = {20, 20};
    dab.colorArgb = 0xFF000000U;
    const auto samples = projectBrushDabs({dab}, rasterizer);
    const auto *sample = at(samples, point);
    if (!sample) return false;

    auto incremental = makeStrokeCompositeBuffer(48, 48);
    accumulateStrokeSamples(incremental, samples);
    const auto single = alpha(strokeCompositePixelAt(incremental, point));
    if (single == 0 || single >= 128) return false;
    for (int i = 0; i < 7; ++i) accumulateStrokeSamples(incremental, samples);
    const auto built = alpha(strokeCompositePixelAt(incremental, point));
    // Repeated coverage at a soft edge must build above a single dab, while
    // the whole stroke remains bounded by the opacity setting.
    if (built <= single + 20 || built > 128 || sample->opacityCap != 128
            || alpha(sample->argb) != single) return false;

    std::vector<RasterSample> batch;
    for (int i = 0; i < 8; ++i) batch.insert(batch.end(), samples.begin(), samples.end());
    auto combined = makeStrokeCompositeBuffer(48, 48);
    accumulateStrokeSamples(combined, batch);
    if (strokeCompositePixelAt(combined, point) != strokeCompositePixelAt(incremental, point)) return false;

    RasterLayer painted = makeRasterLayer(48, 48, 0xFFFFFFFFU);
    compositeStrokeBufferOntoLayer(painted, incremental);
    const auto once = rasterLayerPixelAt(painted, point);
    compositeStrokeBufferOntoLayer(painted, incremental);
    const auto twice = rasterLayerPixelAt(painted, point);
    return ((twice >> 16U) & 0xFFU) < ((once >> 16U) & 0xFFU);
}
}

int main()
{
    Rasterizer procedural;
    procedural.proceduralTip = true;
    procedural.radius = 10;
    procedural.hardness = 0;
    CHECK(checkTip(procedural, {20, 25}));

    // Moving dabs build a darker soft edge than a dot, as in the reference.
    procedural.opacity = .5;
    BrushDab dot;
    dot.position = {20, 20};
    dot.colorArgb = 0xFF000000U;
    auto dotBuffer = makeStrokeCompositeBuffer(48, 48);
    accumulateStrokeSamples(dotBuffer, projectBrushDabs({dot}, procedural));
    std::vector<BrushDab> line;
    for (int x = 12; x <= 28; x += 2) {
        dot.position.x = x;
        line.push_back(dot);
    }
    auto lineBuffer = makeStrokeCompositeBuffer(48, 48);
    accumulateStrokeSamples(lineBuffer, projectBrushDabs(line, procedural));
    CHECK(alpha(strokeCompositePixelAt(lineBuffer, {20, 25}))
          > alpha(strokeCompositePixelAt(dotBuffer, {20, 25})) + 20);
    CHECK(alpha(strokeCompositePixelAt(lineBuffer, {20, 25})) <= 128);

    Rasterizer circle;
    circle.radius = 10;
    CHECK(checkTip(circle, {30, 20}));

    Rasterizer image;
    image.brushWidth = image.brushHeight = 1;
    image.brushAlpha = {128};
    CHECK(checkTip(image, {20, 20}));

    // With a full tip, opacity limits the stroke while flow controls buildup.
    procedural.opacity = .5;
    BrushDab lowFlow;
    lowFlow.position = {20, 20};
    lowFlow.colorArgb = 0xFF000000U;
    lowFlow.alpha = .2;
    const auto samples = projectBrushDabs({lowFlow}, procedural);
    const auto *center = at(samples, {20, 20});
    CHECK(center && center->opacityCap == 128 && alpha(center->argb) == 26);
    auto buffer = makeStrokeCompositeBuffer(48, 48);
    for (int i = 0; i < 4; ++i) accumulateStrokeSamples(buffer, samples);
    const auto four = alpha(strokeCompositePixelAt(buffer, {20, 20}));
    CHECK(four >= 74 && four <= 77);
    for (int i = 0; i < 100; ++i) accumulateStrokeSamples(buffer, samples);
    CHECK(alpha(strokeCompositePixelAt(buffer, {20, 20})) == 128);

    // A later lower dynamic limit cannot remove already deposited pigment.
    lowFlow.opacityCapScale = .1;
    const auto lowerCap = projectBrushDabs({lowFlow}, procedural);
    accumulateStrokeSamples(buffer, lowerCap);
    CHECK(alpha(strokeCompositePixelAt(buffer, {20, 20})) == 128);

    // Transparent source colors reduce the overall stroke limit as well.
    lowFlow.opacityCapScale = 1;
    lowFlow.colorArgb = 0x80000000U;
    lowFlow.alpha = 1;
    const auto translucent = projectBrushDabs({lowFlow}, procedural);
    center = at(translucent, {20, 20});
    CHECK(center && center->opacityCap == 64 && alpha(center->argb) == 64);

    procedural.opacity = 0;
    CHECK(projectBrushDabs({lowFlow}, procedural).empty());
    return 0;
}
