#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include "Brush/BrushResolve.h"
#include "Brush/BrushPresetSerializer.h"
#include "Brush/BrushSnapshot.h"
#include "Document/DocumentSerializer.h"
#include "RasterDabTestUtils.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace {
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
std::uint8_t alphaAt(const std::vector<RasterSample> &pixels, int x, int y)
{
    for (const auto &p : pixels) if (p.position.x == x && p.position.y == y) return p.argb >> 24;
    return 0;
}
std::vector<RasterSample> stamp(const BrushPreset &p, unsigned seed = 17)
{
    const auto r = resolveBrushPreset(p, 0xFFFF0000U, seed);
    if (!r.brush) return {};
    RasterDabStream stream;
    const auto dabs = appendRasterDabs(stream, StrokePoint{{40, 40}}, *r.brush);
    return projectBrushDabs(dabs, r.brush->rasterizer, {}, r.brush->material);
}
std::uint32_t blueSource(const void *, DevicePixelPoint) { return 0xFF0000FFU; }
}

int main()
{
    // Named controls have a diameter floor and count actual emitted stamps for Fade.
    BrushDynamics d;
    d.sizeVariation.enabled = true;
    d.sizeVariation.control = BrushDynamicsControl::PenPressure;
    d.sizeVariation.minimum = .2;
    BrushDynamicsInput input;
    input.pressure = .25;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, .4));
    d.sizeVariation.control = BrushDynamicsControl::Fade;
    d.sizeVariation.fadeSteps = 4;
    input.dabIndex = 2;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, .6));
    input.dabIndex = 9;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, .2));
    d.sizeVariation.control = BrushDynamicsControl::StylusWheel;
    input.tangentialPressure = -1;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, .2));
    input.tangentialPressure = 1;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, 1));

    d = {}; d.sizeVariation.enabled = true; d.sizeVariation.minimum = .2; d.sizeVariation.fadeSteps = 4;
    input = {}; input.pressure = .25; input.tiltX = .3; input.tiltY = .4;
    input.rotationRadians = 3.141592653589793; input.initialDirectionRadians = 1.5707963267948966;
    input.directionRadians = 3.141592653589793; input.dabIndex = 2;
    for (const auto &entry : {std::pair{BrushDynamicsControl::Off, 1.0},
                             std::pair{BrushDynamicsControl::Fade, .6},
                             std::pair{BrushDynamicsControl::PenPressure, .4},
                             std::pair{BrushDynamicsControl::PenTilt, .6},
                             std::pair{BrushDynamicsControl::StylusWheel, .6},
                             std::pair{BrushDynamicsControl::StylusRotation, .6},
                             std::pair{BrushDynamicsControl::InitialDirection, .4},
                             std::pair{BrushDynamicsControl::Direction, .6}}) {
        d.sizeVariation.control = entry.first;
        CHECK(near(resolveBrushDynamics(d, input).sizeScale, entry.second));
    }
    d.pressureInputEnabled = false;
    d.sizeVariation.control = BrushDynamicsControl::PenPressure;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, 1));

    // Independent jitter channels remain deterministic and are neutral when disabled.
    d = {};
    d.sizeVariation.enabled = d.roundnessVariation.enabled = true;
    d.sizeVariation.jitter = d.roundnessVariation.jitter = .8;
    input.randomGrain = .3; input.strokeRandom = .4; input.dabIndex = 3;
    const auto jitter = resolveBrushDynamics(d, input);
    CHECK(jitter.sizeScale < 1 && jitter.roundnessScale < 1);
    CHECK(!near(jitter.sizeScale, jitter.roundnessScale));
    CHECK(near(jitter.sizeScale, resolveBrushDynamics(d, input).sizeScale));
    d.randomInputEnabled = false;
    CHECK(near(resolveBrushDynamics(d, input).sizeScale, 1));

    // Brush pose substitutes only the selected sensor channels before all mappings.
    d = {};
    d.pose.enabled = true;
    d.pose.overridePressure = true; d.pose.pressure = .8;
    d.pose.overrideTiltX = true; d.pose.tiltX = .6;
    d.pose.overrideTiltY = true; d.pose.tiltY = .8;
    d.pressureToFlow = 1;
    CHECK(near(resolveBrushDynamics(d, input).flowScale, .8));
    d.roundnessVariation.enabled = true;
    d.roundnessVariation.control = BrushDynamicsControl::PenTilt;
    CHECK(near(resolveBrushDynamics(d, input).roundnessScale, 1));
    BrushPreset p;
    p.dynamics = d;
    p.dynamics.pose.overrideRotation = true; p.dynamics.pose.rotationRadians = .7;
    p.shape.angleMode = BrushAngleMode::StylusRotation;
    auto resolved = resolveBrushPreset(p);
    CHECK(resolved.brush);
    RasterDabStream stream;
    CHECK(near(appendRasterDabs(stream, StrokePoint{}, *resolved.brush).front().rotationRadians, .7));

    // Initial direction survives a corner while keeping only the previous input point.
    p = {}; p.stroke.spacing = 2; p.shape.angleMode = BrushAngleMode::InitialStrokeDirection;
    resolved = resolveBrushPreset(p);
    CHECK(resolved.brush);
    resetRasterDabStream(stream);
    appendRasterDabs(stream, StrokePoint{{0, 0}, 1, 0}, *resolved.brush);
    const auto up = appendRasterDabs(stream, StrokePoint{{0, 8}, 1, 1}, *resolved.brush);
    const auto right = appendRasterDabs(stream, StrokePoint{{8, 8}, 1, 2}, *resolved.brush);
    CHECK(!up.empty() && !right.empty());
    CHECK(near(up.front().rotationRadians, 1.5707963267948966));
    CHECK(near(right.front().rotationRadians, up.front().rotationRadians));

    // Purity reaches real pixels; mix jitter observes the existing per-stroke policy.
    p = {}; p.color.enabled = true; p.color.purity = -1;
    CHECK((stamp(p).front().argb & 0xFFFFFFU) == 0xFFFFFFU);
    p.color.purity = 0; p.color.mixJitter = 1; p.color.perStroke = true;
    resolved = resolveBrushPreset(p, 0xFFFF0000U, 13);
    CHECK(resolved.brush);
    const auto colored = streamTestDabs(*resolved.brush, StrokePoint{{0, 0}, 1, 0}, StrokePoint{{20, 0}, 1, 1});
    for (const auto &dab : colored) CHECK(dab.colorArgb == colored.front().colorArgb);
    CHECK(colored.front().colorArgb != 0xFFFF0000U);
    p.color.mixJitter = 0;
    p.dynamics.colorMixVariation.enabled = true;
    p.dynamics.colorMixVariation.control = BrushDynamicsControl::PenPressure;
    resolved = resolveBrushPreset(p, 0xFFFF0000U, 13);
    const auto pressureColors = streamTestDabs(*resolved.brush, StrokePoint{{0,0},0,0}, StrokePoint{{20,0},1,1});
    for (const auto &dab : pressureColors) CHECK(dab.colorArgb == 0xFFFF0000U);
    p.color.perStroke = false;
    resolved = resolveBrushPreset(p, 0xFFFF0000U, 13);
    const auto perTipColors = streamTestDabs(*resolved.brush, StrokePoint{{0,0},0,0}, StrokePoint{{20,0},1,1});
    CHECK(perTipColors.back().colorArgb == 0xFFFFFFFFU);

    // Tone, inversion, depth, and mode operate on texture samples, not just metadata.
    p = {}; p.size = 20; p.shape.angleMode = BrushAngleMode::Fixed;
    auto &t = p.material.texture;
    t.enabled = true; t.width = t.height = 1; t.alpha = {0};
    CHECK(stamp(p).empty());
    t.invert = true;
    CHECK(alphaAt(stamp(p), 40, 40) == 255);
    t.invert = false; t.depth = 0;
    CHECK(alphaAt(stamp(p), 40, 40) == 255);
    t.depth = 1; t.brightness = 1;
    CHECK(alphaAt(stamp(p), 40, 40) == 255);
    t.brightness = 0; t.alpha = {128}; t.contrast = -1;
    CHECK(alphaAt(stamp(p), 40, 40) == 128);
    t.blendMode = BrushTextureBlendMode::Subtract;
    CHECK(alphaAt(stamp(p), 40, 40) == 128);
    t.textureEachTip = true; t.minimumDepth = .4;
    p.dynamics.textureDepthVariation.enabled = true;
    p.dynamics.textureDepthVariation.control = BrushDynamicsControl::PenPressure;
    resolved = resolveBrushPreset(p);
    CHECK(resolved.brush);
    resetRasterDabStream(stream);
    const auto shallow = appendRasterDabs(stream, StrokePoint{{40,40}, 0}, *resolved.brush);
    const auto shallowPixels = projectBrushDabs(shallow, resolved.brush->rasterizer, {}, resolved.brush->material);
    CHECK(alphaAt(shallowPixels, 40, 40) > alphaAt(stamp(p), 40, 40));
    // Large valid texture offsets are wrapped before conversion to an integer pixel.
    p.material.texture.offsetX = 1e9; p.material.texture.scale = .001;
    CHECK(!stamp(p).empty());

    p = {}; p.size = 20;
    auto &dual = p.material.dualBrush;
    dual.enabled = true; dual.width = 3; dual.height = 1; dual.alpha = {64,128,255};
    CHECK(alphaAt(stamp(p), 41, 40) == 128);
    dual.flipX = true;
    CHECK(alphaAt(stamp(p), 41, 40) == 255);
    resolved = resolveBrushPreset(p, 0xFFFF0000U, 17);
    resetRasterDabStream(stream);
    const auto dualDabs = appendRasterDabs(stream, StrokePoint{{40,40}}, *resolved.brush);
    const auto zoomedDual = projectBrushDabs(dualDabs, resolved.brush->rasterizer, {{},{},2}, resolved.brush->material);
    CHECK(alphaAt(zoomedDual, 82, 80) == 255);
    dual.scatter.enabled = true; dual.scatter.radius = 2;
    const auto singleDual = stamp(p);
    dual.scatter.count = 8;
    const auto manyDual = stamp(p);
    bool moreCoverage = false;
    for (const auto &pixel : singleDual) {
        const auto beforeAlpha = pixel.argb >> 24;
        const auto afterAlpha = alphaAt(manyDual, pixel.position.x, pixel.position.y);
        CHECK(afterAlpha >= beforeAlpha);
        moreCoverage |= afterAlpha > beforeAlpha;
    }
    CHECK(moreCoverage);

    // Wet edges and noise alter coverage reproducibly, without storing a stroke path.
    p = {}; p.size = 20; p.hardness = 0;
    const auto plain = stamp(p);
    p.stroke.wetEdgesEnabled = true; p.stroke.wetEdgesStrength = .7;
    const auto wet = stamp(p);
    CHECK(alphaAt(wet, 40, 40) < alphaAt(plain, 40, 40));
    CHECK(static_cast<double>(alphaAt(wet, 46, 40)) / alphaAt(wet, 40, 40)
          > static_cast<double>(alphaAt(plain, 46, 40)) / alphaAt(plain, 40, 40));
    p.stroke.wetEdgesEnabled = false; p.stroke.noiseEnabled = true;
    const auto noisy = stamp(p, 21);
    CHECK(alphaAt(noisy, 40, 40) < alphaAt(plain, 40, 40));
    CHECK(alphaAt(noisy, 40, 40) == alphaAt(stamp(p, 21), 40, 40));
    CHECK(alphaAt(noisy, 40, 40) != alphaAt(stamp(p, 22), 40, 40));

    // Tip extensions produce distinct bitmap footprints and expose scalar wear.
    p = {}; p.material.bristle.enabled = true; p.material.bristle.thickness = .2;
    CHECK(alphaAt(stamp(p), 40, 46) == 0);
    p = {}; p.material.erodible.enabled = true; p.material.erodible.softness = 1;
    p.stroke.spacing = 4;
    resolved = resolveBrushPreset(p);
    CHECK(resolved.brush);
    const auto eroded = streamTestDabs(*resolved.brush, StrokePoint{{0,0},1,0}, StrokePoint{{160,0},1,1});
    CHECK(eroded.back().tipWear > eroded.front().tipWear);
    p.material.erodible.wear = .8; sharpenErodibleTip(p.material.erodible);
    CHECK(p.material.erodible.wear == 0);
    p = {}; p.material.airbrushTip.enabled = true; p.material.airbrushTip.granularity = .8;
    CHECK(alphaAt(stamp(p), 40, 40) < 255);
    p.material.airbrushTip.spatter = .5; p.material.airbrushTip.spatterSize = 3;
    CHECK(stamp(p).size() < plain.size());

    // Named mix control changes sampled pigment; load zero deposits no pixels.
    p = {}; p.material.simulation.enabled = true; p.material.simulation.model = BrushSimulationModel::Mixer;
    p.material.simulation.wetness = 1; p.material.simulation.pickup = .5;
    p.material.simulation.mixStrength = 1; p.material.simulation.deposit = .2;
    p.dynamics.mixVariation.enabled = true; p.dynamics.mixVariation.control = BrushDynamicsControl::PenPressure;
    resolved = resolveBrushPreset(p, 0xFFFF0000U);
    RasterSourceSampler source{nullptr, blueSource, {}, 128, 128};
    resetRasterDabStream(stream);
    const auto noMixDabs = appendRasterDabs(stream, StrokePoint{{40,40},0}, *resolved.brush);
    const auto noMix = projectBrushDabs(noMixDabs, resolved.brush->rasterizer, {}, source, resolved.brush->material);
    resetRasterDabStream(stream);
    const auto fullMixDabs = appendRasterDabs(stream, StrokePoint{{40,40},1}, *resolved.brush);
    const auto fullMix = projectBrushDabs(fullMixDabs, resolved.brush->rasterizer, {}, source, resolved.brush->material);
    CHECK(!noMix.empty() && !fullMix.empty());
    CHECK((noMix.front().argb & 0xFFFFFFU) != (fullMix.front().argb & 0xFFFFFFU));
    p.material.simulation.load = 0;
    CHECK(stamp(p).empty());

    // Causal smoothing needs no delayed points or future trajectory.
    p = {}; p.stroke.spacing = 1; p.stroke.smoothingEnabled = true; p.stroke.smoothing = .5;
    resolved = resolveBrushPreset(p);
    CHECK(resolved.brush);
    resetRasterDabStream(stream);
    appendRasterDabs(stream, StrokePoint{{0,0},1,0}, *resolved.brush);
    CHECK(!appendRasterDabs(stream, StrokePoint{{10,0},1,1}, *resolved.brush).empty());
    CHECK(stream.previousPoint.position.x > 0 && stream.previousPoint.position.x < 10);

    // Protect Texture retains pattern identity and scale on preset selection.
    BrushPreset current;
    current.stroke.protectTexture = true;
    current.material.texture.enabled = true;
    current.material.texture.width = current.material.texture.height = 1;
    current.material.texture.alpha = {42}; current.material.texture.scale = 3;
    p = {}; p.material.texture.brightness = .3;
    auto selected = selectBrushPreset(current, p);
    CHECK(selected.material.texture.alpha == current.material.texture.alpha);
    CHECK(selected.material.texture.scale == 3 && selected.material.texture.brightness == .3);
    CHECK(selected.stroke.protectTexture);
    CHECK(selectBrushPreset(current, p, false).material.texture.alpha.empty());

    // All options survive preset, snapshot, and document boundaries. Versions 1/2 remain readable.
    p = selected; p.dynamics = d;
    p.dynamics.mixVariation.enabled = true; p.dynamics.mixVariation.jitter = .4;
    p.material.dualBrush.flipX = true; p.material.dualBrush.scatter.enabled = true;
    p.material.dualBrush.scatter.count = 3; p.material.dualBrush.scatter.radius = 2;
    p.material.simulation.load = .4;
    p.material.bristle.angleRadians = .2; p.material.bristle.thickness = .3;
    p.material.erodible.enabled = true; p.material.erodible.wear = .6;
    p.material.airbrushTip.spatter = .6; p.material.airbrushTip.spatterSize = 4;
    p.stroke.noiseEnabled = true; p.stroke.wetEdgesEnabled = true;
    p.stroke.smoothingEnabled = true;
    p.color.purity = -.3; p.color.mixJitter = .7;
    for (auto member : {&BrushDynamics::sizeVariation, &BrushDynamics::angleVariation,
                        &BrushDynamics::roundnessVariation, &BrushDynamics::scatterVariation,
                        &BrushDynamics::scatterCountVariation, &BrushDynamics::textureDepthVariation,
                        &BrushDynamics::colorMixVariation, &BrushDynamics::opacityVariation,
                        &BrushDynamics::flowVariation, &BrushDynamics::wetnessVariation,
                        &BrushDynamics::mixVariation}) {
        auto &variation = p.dynamics.*member;
        variation.enabled = true; variation.control = BrushDynamicsControl::Fade;
        variation.minimum = .15; variation.jitter = .25; variation.fadeSteps = 11;
    }
    p.dynamics.flipXJitter = p.dynamics.flipYJitter = p.dynamics.projectTip = true;
    p.material.texture.invert = true; p.material.texture.contrast = -.2;
    p.material.texture.textureEachTip = true; p.material.texture.depth = .6; p.material.texture.minimumDepth = .1;
    p.material.texture.blendMode = BrushTextureBlendMode::LinearHeight;
    const auto payload = serializeBrushPreset(p);
    CHECK(payload.find("formatVersion\t3\n") != std::string::npos);
    CHECK(serializeBrushPreset(deserializeBrushPreset(payload)) == payload);
    CHECK(serializeBrushPreset(restoreBrushPreset(snapshotBrushPreset(p))) == payload);
    DocumentArchive archive;
    archive.brushSources.push_back(snapshotBrushPreset(p));
    const auto doc = deserializeDocumentArchive(serializeDocumentArchive(archive));
    CHECK(doc.compatible && doc.brushSources.size() == 1);
    CHECK(serializeBrushPreset(restoreBrushPreset(doc.brushSources[0])) == payload);
    const std::string legacy = "formatMagic\t\"iiPaintBrushPreset\"\nformatVersion\t2\nsize\t12\nflow\t1\nopacity\t1\nhardness\t1\ndensity\t1\n";
    auto old = readBrushPreset(legacy);
    CHECK(old.preset && !old.preset->stroke.noiseEnabled && !old.preset->dynamics.pose.enabled);
    CHECK(old.preset->material.texture.depth == 1 && old.preset->material.bristle.thickness == 1);

    // Validation applies even to disabled fields so invalid state cannot leak through storage.
    p.dynamics.sizeVariation.fadeSteps = 0;
    CHECK(!resolveBrushPreset(p).brush);
    p.dynamics.sizeVariation.fadeSteps = 25;
    p.material.texture.brightness = std::numeric_limits<double>::quiet_NaN();
    CHECK(!resolveBrushPreset(p).brush);
    p.material.texture.brightness = 0; p.dynamics.pose.pressure = 2;
    CHECK(!resolveBrushPreset(p).brush);
    return 0;
}
