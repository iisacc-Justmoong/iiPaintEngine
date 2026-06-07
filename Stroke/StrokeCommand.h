//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <vector>

#include "Brush/BrushDynamics.h"
#include "Brush/BrushMaterial.h"
#include "Core/PaintRect.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCurve.h"
#include "Stroke/StrokeGeometry.h"
#include "Stroke/StrokeInput.h"
#include "Stroke/StrokeResampler.h"

using RawSample = StrokePoint;

struct StrokePath {
    StrokeInput rawInput;
    StrokeInput renderedInput;
    StrokeCurve renderedCurve;
    StrokeGeometryReport rawGeometry;
    StrokeGeometryReport renderedGeometry;
};

struct BrushState {
    Rasterizer rasterizer;
    BrushDynamics dynamics;
    StrokeResampler resampler;
    BrushMaterial material;
    std::uint32_t randomSeed = 0;
};

struct StrokeCommand {
    StrokePath path;
    BrushState brush;
    std::vector<DabCommand> dabs;
    std::vector<DocumentRect> dabDirtyBounds;
    DocumentRect dirtyBounds{};
};

StrokeCommand makeStrokeCommand(const StrokeInput &rawInput,
                                const BrushState &brush,
                                const Stabilizer &stabilizer);
