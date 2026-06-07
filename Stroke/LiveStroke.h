//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Core/PaintRect.h"
#include "Core/RasterSample.h"
#include "Stroke/StrokeCommand.h"

struct LiveStrokeFrame {
    bool active = false;
    StrokeInput rawInput;
    StrokeInput displayedInput;
    StrokeCurve displayedCurve;
    StrokeGeometryReport rawGeometry;
    StrokeGeometryReport displayedGeometry;
    std::vector<DabCommand> dabs;
    std::vector<DocumentRect> dabDirtyBounds;
    DocumentRect documentDirtyBounds{};
    DocumentPoint cursorPreviewPosition{};
    bool cursorPreviewPositionValid = false;
    std::vector<RasterSample> samples;
    DevicePixelRect dirtyBounds{};
};

struct LiveStrokeBuffer {
    bool active = false;
    LiveStrokeFrame frame;
};

LiveStrokeFrame makeLiveStrokeFrame(const StrokeInput &rawInput,
                                    const BrushState &brush,
                                    const Stabilizer &stabilizer,
                                    bool projectSamples = true);

void updateLiveStrokeBuffer(LiveStrokeBuffer &buffer,
                            const StrokeInput &rawInput,
                            const BrushState &brush,
                            const Stabilizer &stabilizer);

void clearLiveStrokeBuffer(LiveStrokeBuffer &buffer);
