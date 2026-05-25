//
// Created by Justmoong on 2026 May 25.
//

#pragma once

#include <vector>

#include "Core/RasterSample.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCommand.h"

struct CanvasLiveStrokeWorkRequest {
    StrokeInput rawInput;
    BrushState brush;
    Stabilizer stabilizer;
    RasterProjection projection;
};

struct CanvasLiveStrokeWorkResult {
    LiveStrokeFrame frame;
    std::vector<RasterSample> samples;
    DevicePixelRect dirtyBounds{};
};

struct CanvasCommitStrokeWorkRequest {
    StrokeInput rawInput;
    BrushState brush;
    Stabilizer stabilizer;
    RasterProjection projection;
};

struct CanvasCommitStrokeWorkResult {
    StrokeCommand command;
    std::vector<RasterSample> samples;
    DevicePixelRect dirtyBounds{};
};

CanvasLiveStrokeWorkResult runCanvasLiveStrokeWork(const CanvasLiveStrokeWorkRequest &request);

CanvasCommitStrokeWorkResult runCanvasCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request);
