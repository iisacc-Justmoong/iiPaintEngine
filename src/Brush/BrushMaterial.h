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
};

struct BrushScatter {
    bool enabled = false;
    Types::Scalar radius = 0.0;
    std::uint32_t count = 1;
};

struct BrushSimulation {
    bool enabled = false;
    BrushSimulationModel model = BrushSimulationModel::Dry;
    Types::Scalar wetness = 0.0;
    Types::Scalar smudgeStrength = 0.0;
    Types::Scalar mixStrength = 0.0;
    Types::Scalar pickup = 0.0;
    Types::Scalar deposit = 1.0;
};

struct BristleSimulation {
    bool enabled = false;
    BristleShape shape = BristleShape::Round;
    std::uint32_t count = 0;
    Types::Scalar length = 0.0;
    Types::Scalar stiffness = 1.0;
};

struct BrushMaterial {
    BrushTexture texture;
    BrushTexture paperGrain;
    DualBrush dualBrush;
    BrushScatter scatter;
    BrushSimulation simulation;
    BristleSimulation bristle;
};
