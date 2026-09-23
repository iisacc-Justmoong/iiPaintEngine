#include <cmath>
#include <iostream>
#include <limits>

#include "Brush/BrushLibrary.h"
#include "Brush/BrushResolve.h"
#include "Brush/BrushPresetSerializer.h"
#include "Document/DocumentSerializer.h"
#include "RasterDabTestUtils.h"

#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": " #condition "\n"; return 1; } } while (false)

int main()
{
    BrushPreset preset;
    preset.name = "Expressive ink";
    preset.size = 20;
    preset.shape.angleMode = BrushAngleMode::StylusRotation;
    preset.shape.roundness = 0.25;
    preset.stroke.spacingRatio = 0.25;
    preset.material.scatter.enabled = true;
    preset.material.scatter.count = 3;
    preset.material.scatter.radius = 4;
    preset.material.scatter.axes = BrushScatterAxes::Perpendicular;
    BrushDynamicsBinding pressure;
    pressure.target = BrushDynamicsTarget::Size;
    pressure.curve.points = {{0, 0.1}, {0.25, 0.4}, {1, 1}};
    preset.dynamics.bindings.push_back(pressure);
    auto resolved = resolveBrushPreset(preset, 0xFF336699U, 42);
    CHECK(resolved.brush.has_value());
    StrokePoint first{{30, 30}, 0.25, 0};
    first.rotationRadians = 0.5;
    RasterDabStream stream;
    const auto dabs = appendRasterDabs(stream, first, *resolved.brush);
    CHECK(dabs.size() == 3);
    CHECK(std::abs(dabs[0].rotationRadians - 0.5) < 1e-9);
    CHECK(std::abs(dabs[0].ellipseScaleY - 0.25) < 1e-9);
    CHECK(dabs[0].position.x == 30);
    CHECK(!projectBrushDabs(dabs, resolved.brush->rasterizer, {}, resolved.brush->material).empty());
    RasterDabStream repeat;
    const auto repeated = appendRasterDabs(repeat, first, *resolved.brush);
    CHECK(dabs[1].position.y == repeated[1].position.y);

    preset.material.scatter.enabled = false;
    preset.stroke.airbrushEnabled = true;
    preset.stroke.airbrushRate = 10;
    resolved = resolveBrushPreset(preset);
    CHECK(resolved.brush);
    resetRasterDabStream(stream);
    CHECK(appendRasterDabs(stream, first, *resolved.brush).size() == 1);
    first.time = 0.5;
    CHECK(appendRasterDabs(stream, first, *resolved.brush).size() == 5);
    CHECK(appendRasterDabs(stream, first, *resolved.brush, true).empty());
    CHECK(!stream.active);

    const auto payload = serializeBrushPreset(preset);
    const auto decoded = readBrushPreset(payload);
    CHECK(decoded.preset);
    CHECK(decoded.preset->dynamics.bindings[0].curve.points.size() == 3);
    CHECK(decoded.preset->stroke.airbrushRate == 10);
    CHECK(serializeBrushPreset(*decoded.preset) == payload);
    CHECK(!readBrushPreset("formatMagic\t\"iiPaintBrushPreset\"\nformatVersion\t999\n").preset);
    preset.size = std::numeric_limits<float>::quiet_NaN();
    CHECK(!resolveBrushPreset(preset).brush);

    for (const auto &builtIn : builtInBrushPresets()) {
        const auto brush = resolveBrushPreset(builtIn);
        CHECK(brush.brush);
        CHECK(readBrushPreset(serializeBrushPreset(builtIn)).preset);
    }
    CHECK(builtInBrushPresets().size() >= 6);
    return 0;
}
