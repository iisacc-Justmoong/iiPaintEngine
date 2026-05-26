//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <cstdint>
#include <vector>

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

struct BrushTexture {
    bool enabled = false;
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<Types::Byte> alpha;
    Types::Scalar grainStrength = 0.0;
    Types::Scalar scale = 1.0;
};

struct DualBrush {
    bool enabled = false;
    Types::Scalar scale = 1.0;
    Types::Scalar spacingRatio = 1.0;
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
    BrushSimulationModel model = BrushSimulationModel::Dry;
    Types::Scalar wetness = 0.0;
    Types::Scalar smudgeStrength = 0.0;
    Types::Scalar mixStrength = 0.0;
};

struct BristleSimulation {
    BristleShape shape = BristleShape::Round;
    std::uint32_t count = 0;
    Types::Scalar length = 0.0;
    Types::Scalar stiffness = 1.0;
};

struct BrushMaterial {
    BrushTexture texture;
    DualBrush dualBrush;
    BrushScatter scatter;
    BrushSimulation simulation;
    BristleSimulation bristle;
};
