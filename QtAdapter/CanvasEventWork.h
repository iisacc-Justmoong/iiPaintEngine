//
// Created by Justmoong on 2026 May 25.
//

#pragma once

#include <vector>

#include "Core/RasterSample.h"
#include "Layer/RasterLayer.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeCommand.h"

struct CanvasLiveStrokeWorkRequest {
    StrokeInput rawInput;
    BrushState brush;
    Stabilizer stabilizer;
    RasterProjection projection;
    RasterLayer sourceLayer;
    DevicePixelPoint sourceLayerOrigin{};
    Types::Scalar incrementalPreviewStartDistance = 0.0;
    bool sourceLayerEnabled = false;
    bool incrementalPreviewEnabled = false;
};

struct CanvasLiveStrokeWorkResult {
    LiveStrokeFrame frame;
    std::vector<RasterSample> samples;
    DevicePixelRect dirtyBounds{};
    DevicePixelRect fullDirtyBounds{};
    Types::Scalar renderedStrokeDistance = 0.0;
    Types::Scalar incrementalPreviewStartDistance = 0.0;
    bool incrementalPreview = false;
};

struct CanvasCommitStrokeWorkRequest {
    StrokeInput rawInput;
    BrushState brush;
    Stabilizer stabilizer;
    RasterProjection projection;
    RasterLayer sourceLayer;
    DevicePixelPoint sourceLayerOrigin{};
    bool sourceLayerEnabled = false;
};

struct CanvasCommitStrokeWorkResult {
    StrokeCommand command;
    std::vector<RasterSample> samples;
    DevicePixelRect dirtyBounds{};
};

CanvasLiveStrokeWorkResult runCanvasLiveStrokeWork(const CanvasLiveStrokeWorkRequest &request);

CanvasCommitStrokeWorkResult runCanvasCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request);

bool brushNeedsSourceLayer(const BrushState &brush);
