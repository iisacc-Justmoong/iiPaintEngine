#pragma once

#include <cstdint>
#include <vector>
#include "Brush/BrushDynamics.h"
#include "Brush/BrushMaterial.h"
#include "Brush/BrushShape.h"
#include "Brush/BrushSettings.h"
#include "Brush/BrushTip.h"
#include "Core/RasterBlendMode.h"

struct Rasterizer {
    Types::Pixel radius = 2;
    std::uint32_t argb = 0xFF000000U;
    Types::Scalar brushSize = 0.0;
    Types::Pixel brushWidth = 0;
    Types::Pixel brushHeight = 0;
    std::vector<Types::Byte> brushAlpha;
    Types::Scalar spacing = 0.0;
    Types::Scalar spacingRatio = 0.0;
    bool spacingEnabled = true;
    Types::Scalar opacity = 1.0;
    bool opacityEnabled = true;
    Types::Scalar flow = 1.0;
    bool flowEnabled = true;
    Types::Scalar hardness = 1.0;
    bool hardnessEnabled = true;
    Types::Scalar density = 1.0;
    Types::Scalar pressureScale = 0.0;
    Types::Scalar velocitySpacing = 0.0;
    Types::Scalar warmupDistance = 0.0;
    Types::Scalar taperMinimum = 0.25;
    StrokeTaperShape warmupTaperShape = StrokeTaperShape::Linear;
    Types::Scalar rotationJitter = 0.0;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
    BrushShape shape;
    BrushTipSequence tipSequence;
    BrushColorSettings color;
    bool normalizeTipSize = false;
    bool proceduralTip = false;
    bool spacingFollowsSize = false;
    bool airbrushEnabled = false;
    Types::Scalar airbrushRate = 30.0;
};

struct BrushState {
    Rasterizer rasterizer;
    BrushDynamics dynamics;
    BrushMaterial material;
    std::uint32_t randomSeed = 0;
};
