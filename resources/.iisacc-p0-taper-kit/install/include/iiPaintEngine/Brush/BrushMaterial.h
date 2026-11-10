//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/PaintUuid.h"
#include "Core/Types.h"

enum class BrushSimulationModel {
    Dry,
    WetPaint,
    Smudge,
    Mixer,
};

enum class BristleShape {
    Round,
    Flat,
    Fan,
};

enum class BrushTextureSpace {
    Tip,
    StrokeFollow,
    Document,
    Paper,
};

enum class DualBrushCompositeMode {
    Multiply,
    Add,
    Subtract,
    Difference,
    Darken, Lighten, ColorDodge, ColorBurn, LinearBurn, HardMix,
};

enum class BrushTextureBlendMode {
    Multiply, Subtract, Darken, Lighten, ColorDodge, ColorBurn,
    LinearBurn, HardMix, Height, LinearHeight,
};

struct BrushTextureAssetCache {
    bool enabled = false;
    PaintUuid assetId;
    std::string cacheKey;
    std::uint64_t revision = 0;
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<Types::Byte> alpha;
};

struct BrushTexture {
    bool enabled = false;
    BrushTextureSpace space = BrushTextureSpace::Tip;
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<Types::Byte> alpha;
    Types::Scalar grainStrength = 0.0;
    Types::Scalar strength = 1.0;
    Types::Scalar scale = 1.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar offsetX = 0.0;
    Types::Scalar offsetY = 0.0;
    Types::Scalar scaleJitter = 0.0;
    Types::Scalar rotationJitter = 0.0;
    BrushTextureAssetCache assetCache;
    bool invert = false;
    Types::Scalar brightness = 0.0; // Normalized signed tone adjustment, [-1,1].
    Types::Scalar contrast = 0.0;
    Types::Scalar depth = 1.0;
    Types::Scalar minimumDepth = 0.0;
    bool textureEachTip = false; // Overrides the configured coordinate space with Tip.
    BrushTextureBlendMode blendMode = BrushTextureBlendMode::Multiply;
};

enum class BrushScatterAxes { Both, Perpendicular, AlongStroke };
enum class BrushScatterDistribution { Square, Disk };

struct BrushScatter {
    bool enabled = false;
    Types::Scalar radius = 0.0;
    std::uint32_t count = 1;
    BrushScatterAxes axes = BrushScatterAxes::Both;
    BrushScatterDistribution distribution = BrushScatterDistribution::Square;
    Types::Scalar countJitter = 0.0;
    bool relativeToSize = false;
};

struct DualBrush {
    bool enabled = false;
    DualBrushCompositeMode compositeMode = DualBrushCompositeMode::Multiply;
    Types::Scalar scale = 1.0;
    Types::Scalar spacingRatio = 1.0;
    Types::Scalar opacity = 1.0;
    Types::Scalar rotationRadians = 0.0;
    Types::Scalar offsetX = 0.0;
    Types::Scalar offsetY = 0.0;
    Types::Scalar scaleJitter = 0.0;
    Types::Scalar rotationJitter = 0.0;
    std::vector<Types::Byte> alpha;
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    bool flipX = false;
    bool flipY = false;
    BrushScatter scatter;
};

struct BrushSimulation {
    bool enabled = false;
    BrushSimulationModel model = BrushSimulationModel::Dry;
    Types::Scalar wetness = 0.0;
    Types::Scalar smudgeStrength = 0.0;
    Types::Scalar mixStrength = 0.0;
    Types::Scalar pickup = 0.0;
    Types::Scalar deposit = 1.0;
    Types::Scalar load = 1.0; // Reservoir contribution in the bitmap mixing model.
};

struct BristleSimulation {
    bool enabled = false;
    BristleShape shape = BristleShape::Round;
    std::uint32_t count = 0;
    Types::Scalar length = 0.0;
    Types::Scalar stiffness = 1.0;
    Types::Scalar thickness = 1.0; // Relative width of the 2D footprint, (0,1].
    Types::Scalar angleRadians = 0.0;
};

// Bitmap approximations. These do not reproduce Adobe's native physical tip simulation.
struct ErodibleTip {
    bool enabled = false;
    Types::Scalar softness = 0.5;
    Types::Scalar wear = 0.0; // Caller may carry RasterDabStream::tipWear to the next stroke.
};

void sharpenErodibleTip(ErodibleTip &tip);

struct AirbrushTip {
    bool enabled = false;
    Types::Scalar granularity = 0.0;
    Types::Scalar spatter = 0.0;
    Types::Scalar spatterSize = 1.0; // Document pixels.
    Types::Scalar distortion = 0.0;
};

struct BrushMaterial {
    BrushTexture texture;
    BrushTexture paperGrain;
    DualBrush dualBrush;
    BrushScatter scatter;
    BrushSimulation simulation;
    BristleSimulation bristle;
    ErodibleTip erodible;
    AirbrushTip airbrushTip;
};
