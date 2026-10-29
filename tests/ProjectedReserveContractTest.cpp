// Portable regression candidate: vector capacity is observable without
// interposing a global allocation function across a shared-library boundary.
#include "Stroke/Rasterizer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
std::uint32_t sample(const void *, DevicePixelPoint p)
{
    const auto x = static_cast<std::uint32_t>(p.x);
    const auto y = static_cast<std::uint32_t>(p.y);
    return 0xff000000U | ((x * 13U) & 255U) << 16U | ((y * 7U) & 255U) << 8U | 0x55U;
}

bool verify(std::string_view name, const std::vector<RasterSample> &samples, bool empty)
{
    constexpr std::size_t maximumBytes = 1024 * 1024;
    if (samples.empty() != empty) {
        std::cerr << name << ": unexpected sample count\n";
        return false;
    }
    if (samples.capacity() >= maximumBytes / sizeof(RasterSample)) {
        std::cerr << name << ": small or invisible projected dabs reserved "
                  << samples.capacity() * sizeof(RasterSample) << " bytes\n";
        return false;
    }
    return true;
}
}

int main()
{
    Rasterizer raster;
    raster.brushWidth = 1024; raster.brushHeight = 1024;
    raster.brushSize = 16; raster.normalizeTipSize = true;
    raster.opacity = .8; raster.flow = .9; raster.hardness = .85;
    raster.brushAlpha.resize(1024 * 1024);
    for (int y = 0; y < 1024; ++y)
        for (int x = 0; x < 1024; ++x)
            raster.brushAlpha[std::size_t(y) * 1024 + x] = static_cast<unsigned char>((x * 3 + y * 7) % 256);
    std::array<BrushDab, 2> dabs{};
    dabs[0].position = {30.25, 28.75}; dabs[0].scale = 16.0 / 1024;
    dabs[0].rotationRadians = .37; dabs[0].ellipseScaleX = 1.2; dabs[0].ellipseScaleY = .7;
    dabs[0].alpha = .8; dabs[0].colorArgb = 0xff2360c0;
    dabs[1] = dabs[0]; dabs[1].position = {-10.75, 3.25};
    dabs[1].scale = 8.0 / 1024; dabs[1].rotationRadians = -.8;
    RasterProjection projection;
    projection.deviceOrigin = {-7, 5};
    const BrushMaterial material;
    const RasterSourceSampler source{nullptr, sample, {-64, -64}, 256, 256};
    const BrushDabSpan span{dabs.data(), dabs.size()};
    if (!verify("plain", projectBrushDabs(span, raster, projection), false)) return 1;
    projection.scale = .25;
    if (!verify("material", projectBrushDabs(span, raster, projection, material), false)) return 2;
    projection.scale = 2;
    if (!verify("source", projectBrushDabs(span, raster, projection, source, material), false)) return 3;

    dabs[0].alpha = 0; dabs[1].scale = 0;
    if (!verify("invisible", projectBrushDabs(span, raster, projection), true)) return 4;
    if (!verify("empty", projectBrushDabs(BrushDabSpan{}, raster, projection), true)) return 5;

    // Coordinates are valid for the existing projection path, but the draft's
    // cautious estimator declines to reserve for them.
    dabs[0].position = {-1'000'000'000.0, 1'000'000'000.0};
    dabs[0].alpha = 1;
    dabs[0].scale = 4.0 / 1024;
    projection = {};
    if (!verify("far", projectBrushDabs(BrushDabSpan{dabs.data(), 1}, raster, projection), false)) return 6;
    return 0;
}
