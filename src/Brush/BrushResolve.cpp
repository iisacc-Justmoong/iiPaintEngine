//
// Created by Justmoong on 2026 May 24.
//

#include "BrushResolve.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr std::size_t maxMaskBytes = 16 * 1024 * 1024;
struct Validator {
    std::vector<std::string> errors;
    void range(const std::string &key, double value, double low, double high)
    {
        if (!std::isfinite(value) || value < low || value > high) errors.push_back(key + ": outside supported range");
    }
    template<class T> void enumeration(const std::string &key, T value, T last)
    {
        range(key, static_cast<int>(value), 0, static_cast<int>(last));
    }
    void mask(const std::string &key, int width, int height, std::size_t bytes)
    {
        if (width == 0 && height == 0 && bytes == 0) return;
        if (width <= 0 || height <= 0 || width > 4096 || height > 4096
                || bytes > maxMaskBytes || bytes != static_cast<std::size_t>(width) * height)
            errors.push_back(key + ": invalid mask dimensions or byte count");
    }
    void curve(const std::string &key, const BrushDynamicsResponseCurve &c)
    {
        range(key + ".min", c.min, -1024, 1024);
        range(key + ".center", c.center, -1024, 1024);
        range(key + ".max", c.max, -1024, 1024);
        range(key + ".jitter", c.jitter, 0, 1024);
        enumeration(key + ".easing", c.easing, BrushDynamicsEasing::EaseInOut);
        if (c.points.size() > 64) errors.push_back(key + ": at most 64 curve knots");
        double previous = -1.0;
        for (const auto &point : c.points) {
            range(key + ".input", point.input, 0, 1);
            range(key + ".output", point.output, -1024, 1024);
            if (point.input <= previous) errors.push_back(key + ": curve inputs must strictly increase");
            previous = point.input;
        }
    }
    void response(const std::string &key, const BrushDynamicsPropertyResponse &r)
    {
        range(key + ".neutral", r.neutral, -1024, 1024);
        enumeration(key + ".combineMode", r.combineMode, BrushDynamicsCombineMode::Replace);
        curve(key + ".pressure", r.pressure); curve(key + ".velocity", r.velocity);
        curve(key + ".tilt", r.tilt); curve(key + ".random", r.random);
    }
    void texture(const std::string &key, const BrushTexture &t)
    {
        enumeration(key + ".space", t.space, BrushTextureSpace::Paper);
        mask(key, t.width, t.height, t.alpha.size());
        mask(key + ".assetCache", t.assetCache.width, t.assetCache.height, t.assetCache.alpha.size());
        range(key + ".grainStrength", t.grainStrength, 0, 1); range(key + ".strength", t.strength, 0, 1);
        range(key + ".scale", t.scale, 0.001, 4096); range(key + ".scaleJitter", t.scaleJitter, 0, 1);
        range(key + ".rotation", t.rotationRadians, -1024, 1024); range(key + ".rotationJitter", t.rotationJitter, 0, 1024);
        range(key + ".offsetX", t.offsetX, -1e9, 1e9); range(key + ".offsetY", t.offsetY, -1e9, 1e9);
        if (t.assetCache.cacheKey.size() > 8192) errors.push_back(key + ": cache key too long");
    }
};
}

std::vector<std::string> validateBrushPreset(const BrushPreset &p)
{
    Validator v;
    // Zero-sized legacy presets may be archived, but cannot be resolved for painting.
    v.range("size", p.size, 0, 4096); v.range("opacity", p.opacity, 0, 1);
    v.range("flow", p.flow, 0, 1); v.range("hardness", p.hardness, 0, 1); v.range("density", p.density, 0, 1024);
    if (p.name.size() > 8192) v.errors.push_back("name: too long");
    v.mask("tip", p.tip.width, p.tip.height, p.tip.mask.size());
    v.enumeration("tipSelection", p.tipSequence.selection, BrushTipSelection::Pressure);
    if (p.tipSequence.tips.size() > 256) v.errors.push_back("tips: at most 256 masks");
    std::size_t totalBytes = p.tip.mask.size();
    for (const auto &tip : p.tipSequence.tips) {
        v.mask("tips", tip.width, tip.height, tip.mask.size());
        if (tip.mask.empty()) v.errors.push_back("tips: each tip must contain a mask");
        totalBytes += tip.mask.size();
    }
    const auto &m = p.material;
    totalBytes += m.texture.alpha.size() + m.texture.assetCache.alpha.size()
            + m.paperGrain.alpha.size() + m.paperGrain.assetCache.alpha.size() + m.dualBrush.alpha.size();
    if (totalBytes > maxMaskBytes) v.errors.push_back("masks: total exceeds 16 MiB");
    v.enumeration("shape.kind", p.shape.kind, BrushTipShape::Diamond);
    v.enumeration("shape.angleMode", p.shape.angleMode, BrushAngleMode::StylusRotation);
    v.range("shape.angleRadians", p.shape.angleRadians, -1024, 1024);
    v.range("shape.roundness", p.shape.roundness, 0.01, 1);
    const auto &s = p.stroke;
    v.range("spacing", s.spacing, 0, 1e6); v.range("spacingRatio", s.spacingRatio, 0, 100);
    v.range("warmupDistance", s.warmupDistance, 0, 1e9); v.range("taperMinimum", s.taperMinimum, 0, 1);
    v.range("airbrushRate", s.airbrushRate, 0.1, 1000);
    v.enumeration("warmupTaperShape", s.warmupTaperShape, StrokeTaperShape::SmoothStep);
    v.enumeration("blendMode", s.blendMode, RasterBlendMode::Overlay);
    const auto &d = p.dynamics;
    for (auto value : {d.pressureToSize, d.pressureToOpacity, d.pressureToFlow, d.velocityToSpacing,
                      d.velocityToOpacity, d.velocityToDryOut, d.tiltToEllipse, d.rotationJitter, d.grainJitter})
        v.range("dynamics", value, 0, 1024);
    v.response("sizeResponse", d.sizeResponse); v.response("opacityResponse", d.opacityResponse);
    v.response("flowResponse", d.flowResponse); v.response("spacingResponse", d.spacingResponse);
    v.response("scatterResponse", d.scatterResponse); v.response("rotationResponse", d.rotationResponse);
    v.response("textureDepthResponse", d.textureDepthResponse); v.response("wetnessResponse", d.wetnessResponse);
    v.response("dryOutResponse", d.dryOutResponse); v.response("bristleSpreadResponse", d.bristleSpreadResponse);
    if (d.bindings.size() > 64) v.errors.push_back("bindings: at most 64 mappings");
    for (const auto &b : d.bindings) {
        v.enumeration("binding.source", b.source, BrushDynamicsSource::Custom);
        v.enumeration("binding.target", b.target, BrushDynamicsTarget::ColorMix);
        v.enumeration("binding.combineMode", b.combineMode, BrushDynamicsCombineMode::Replace);
        v.range("binding.customInput", b.customInput, 0, 7);
        v.range("binding.inputMinimum", b.inputMinimum, -1e9, 1e9);
        v.range("binding.inputMaximum", b.inputMaximum, -1e9, 1e9);
        if (b.inputMaximum <= b.inputMinimum) v.errors.push_back("binding: input range must increase");
        v.curve("binding.curve", b.curve);
    }
    v.texture("texture", m.texture); v.texture("paperGrain", m.paperGrain);
    v.mask("dualBrush", m.dualBrush.width, m.dualBrush.height, m.dualBrush.alpha.size());
    v.enumeration("dualBrush.mode", m.dualBrush.compositeMode, DualBrushCompositeMode::Difference);
    v.range("dualBrush.scale", m.dualBrush.scale, 0.001, 1024);
    v.range("dualBrush.spacingRatio", m.dualBrush.spacingRatio, 0.001, 100);
    v.range("dualBrush.opacity", m.dualBrush.opacity, 0, 1);
    v.range("dualBrush.rotation", m.dualBrush.rotationRadians, -1024, 1024);
    v.range("dualBrush.rotationJitter", m.dualBrush.rotationJitter, 0, 1024);
    v.range("dualBrush.scaleJitter", m.dualBrush.scaleJitter, 0, 1);
    v.range("dualBrush.offsetX", m.dualBrush.offsetX, -1e9, 1e9);
    v.range("dualBrush.offsetY", m.dualBrush.offsetY, -1e9, 1e9);
    v.enumeration("scatter.axes", m.scatter.axes, BrushScatterAxes::AlongStroke);
    v.enumeration("scatter.distribution", m.scatter.distribution, BrushScatterDistribution::Disk);
    v.range("scatter.count", m.scatter.count, 1, 256); v.range("scatter.countJitter", m.scatter.countJitter, 0, 1);
    v.range("scatter.radius", m.scatter.radius, 0, 1e6);
    v.enumeration("simulation.model", m.simulation.model, BrushSimulationModel::Mixer);
    for (auto value : {m.simulation.wetness, m.simulation.smudgeStrength, m.simulation.mixStrength,
                      m.simulation.pickup, m.simulation.deposit}) v.range("simulation", value, 0, 1);
    v.enumeration("bristle.shape", m.bristle.shape, BristleShape::Fan);
    v.range("bristle.count", m.bristle.count, 0, 4096); v.range("bristle.length", m.bristle.length, 0, 4096);
    v.range("bristle.stiffness", m.bristle.stiffness, 0, 1);
    for (auto value : {p.color.mix, p.color.hueJitter, p.color.saturationJitter, p.color.valueJitter})
        v.range("color", value, 0, 1);
    return v.errors;
}

BrushResolve resolveBrushPreset(const BrushPreset &p, std::uint32_t colorArgb, std::uint32_t randomSeed)
{
    BrushResolve result;
    result.errors = validateBrushPreset(p);
    if (p.size <= 0) result.errors.push_back("size: painting requires a positive diameter");
    if (p.density <= 0) result.errors.push_back("density: painting requires a positive density");
    if (!result.errors.empty()) return result;
    BrushState brush;
    brush.dynamics = p.dynamics; brush.material = p.material; brush.randomSeed = randomSeed;
    auto &r = brush.rasterizer;
    r.argb = colorArgb; r.brushSize = p.size; r.radius = 1;
    r.brushWidth = p.tip.width; r.brushHeight = p.tip.height;
    r.brushAlpha.reserve(p.tip.mask.size());
    for (auto byte : p.tip.mask) r.brushAlpha.push_back(std::to_integer<Types::Byte>(byte));
    r.opacity = p.opacity; r.flow = p.flow; r.hardness = p.hardness; r.density = p.density;
    r.shape = p.shape; r.tipSequence = p.tipSequence; r.color = p.color;
    r.normalizeTipSize = true; r.proceduralTip = p.tip.mask.empty() && p.tipSequence.tips.empty();
    r.spacing = p.stroke.spacing; r.spacingRatio = p.stroke.spacingRatio;
    r.spacingEnabled = p.stroke.spacingEnabled; r.spacingFollowsSize = p.stroke.spacingFollowsSize;
    r.flowEnabled = p.stroke.flowEnabled; r.opacityEnabled = p.stroke.opacityEnabled;
    r.hardnessEnabled = p.stroke.hardnessEnabled; r.warmupDistance = p.stroke.warmupDistance;
    r.taperMinimum = p.stroke.taperMinimum; r.warmupTaperShape = p.stroke.warmupTaperShape;
    r.airbrushEnabled = p.stroke.airbrushEnabled; r.airbrushRate = p.stroke.airbrushRate;
    r.blendMode = p.stroke.blendMode;
    result.brush = std::move(brush);
    return result;
}
