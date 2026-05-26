//
// Created by Justmoong on 2026 May 24.
//

#include "LiveStroke.h"

#include <algorithm>

namespace {

DevicePixelRect dirtyBoundsForSamples(const std::vector<RasterSample> &samples)
{
    if (samples.empty()) {
        return {};
    }

    Types::Pixel left = samples.front().position.x;
    Types::Pixel top = samples.front().position.y;
    Types::Pixel right = samples.front().position.x;
    Types::Pixel bottom = samples.front().position.y;

    for (const RasterSample &sample : samples) {
        left = std::min(left, sample.position.x);
        top = std::min(top, sample.position.y);
        right = std::max(right, sample.position.x);
        bottom = std::max(bottom, sample.position.y);
    }

    return DevicePixelRect{
            {left, top},
            std::max<Types::Pixel>(0, right - left + 1),
            std::max<Types::Pixel>(0, bottom - top + 1),
    };
}

} // namespace

LiveStrokeFrame makeLiveStrokeFrame(const StrokeInput &rawInput,
                                    const BrushState &brush,
                                    const Stabilizer &stabilizer,
                                    bool projectSamples)
{
    LiveStrokeFrame frame;
    frame.active = !rawInput.points.empty();
    frame.rawInput = rawInput;
    if (!frame.active) {
        return frame;
    }

    frame.displayedInput = stabilizeStrokeInput(rawInput, stabilizer);
    if (!frame.displayedInput.points.empty() && !stabilizer.previewDabsMatchCursor) {
        frame.displayedInput.points.back() = rawInput.points.back();
    }
    frame.displayedInput = resampleStrokeInput(frame.displayedInput, brush.resampler);

    frame.displayedCurve = makeStrokeCurve(frame.displayedInput);
    frame.dabs = placeBrushDabs(frame.displayedCurve,
                                brush.rasterizer,
                                brush.dynamics,
                                brush.randomSeed);
    frame.dabDirtyBounds = documentBoundsForEachBrushDab(frame.dabs, brush.rasterizer);
    frame.documentDirtyBounds = documentBoundsForBrushDabs(frame.dabs, brush.rasterizer);
    if (stabilizer.previewDabsMatchCursor && !frame.dabs.empty()) {
        frame.cursorPreviewPosition = frame.dabs.back().position;
        frame.cursorPreviewPositionValid = true;
    } else if (!frame.displayedInput.points.empty()) {
        frame.cursorPreviewPosition = frame.displayedInput.points.back().position;
        frame.cursorPreviewPositionValid = true;
    }
    if (projectSamples) {
        frame.samples = projectBrushDabs(frame.dabs, brush.rasterizer);
        frame.dirtyBounds = dirtyBoundsForSamples(frame.samples);
    }
    return frame;
}

void updateLiveStrokeBuffer(LiveStrokeBuffer &buffer,
                            const StrokeInput &rawInput,
                            const BrushState &brush,
                            const Stabilizer &stabilizer)
{
    buffer.frame = makeLiveStrokeFrame(rawInput, brush, stabilizer);
    buffer.active = buffer.frame.active;
}

void clearLiveStrokeBuffer(LiveStrokeBuffer &buffer)
{
    buffer.active = false;
    buffer.frame = {};
}
