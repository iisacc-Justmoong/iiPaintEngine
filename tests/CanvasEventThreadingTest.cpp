#include <cstdint>
#include <future>
#include <thread>
#include <utility>

#include "Layer/RasterLayer.h"
#include "QtAdapter/CanvasEventWork.h"
#include "Render/DirtyRegion.h"

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

StrokeInput makeRawStroke()
{
    StrokeInput stroke;
    stroke.points = {
            {{1.0, 1.0}, 1.0, 0.0},
            {{8.0, 6.0}, 0.8, 1.0},
            {{14.0, 2.0}, 1.0, 2.0},
    };
    return stroke;
}

BrushState makeBrush()
{
    BrushState brush;
    brush.randomSeed = 101;
    brush.rasterizer.brushSize = 4.0;
    brush.rasterizer.radius = 2;
    brush.rasterizer.spacing = 1.0;
    brush.rasterizer.spacingRatio = 0.5;
    brush.rasterizer.flow = 0.7;
    brush.rasterizer.opacity = 0.9;
    brush.rasterizer.hardness = 0.8;
    return brush;
}

RasterProjection makeProjection()
{
    return RasterProjection{{0.0, 0.0}, {0, 0}, 1.0};
}

} // namespace

int main()
{
    const std::thread::id mainThread = std::this_thread::get_id();

    CanvasLiveStrokeWorkRequest liveRequest;
    liveRequest.rawInput = makeRawStroke();
    liveRequest.brush = makeBrush();
    liveRequest.stabilizer = Stabilizer{0.25};
    liveRequest.projection = makeProjection();
    liveRequest.sourceLayer = makeRasterLayer(2048, 2048, 0xFFFF0000U);
    liveRequest.sourceLayerEnabled = false;

    auto liveFuture = std::async(std::launch::async, [mainThread, liveRequest]() {
        const CanvasLiveStrokeWorkResult result = runCanvasLiveStrokeWork(liveRequest);
        return std::pair{std::this_thread::get_id() != mainThread, result};
    });
    const auto [liveRanOffGuiThread, liveResult] = liveFuture.get();
    if (!liveRanOffGuiThread
            || !liveResult.frame.active
            || liveResult.frame.rawInput.points.size() != liveRequest.rawInput.points.size()
            || liveResult.samples.empty()
            || !liveResult.frame.samples.empty()
            || isEmpty(liveResult.dirtyBounds)) {
        return 1;
    }

    CanvasLiveStrokeWorkRequest dryRequest = liveRequest;
    dryRequest.sourceLayerEnabled = false;
    const CanvasLiveStrokeWorkResult dryResult = runCanvasLiveStrokeWork(dryRequest);
    dryRequest.sourceLayerEnabled = true;
    const CanvasLiveStrokeWorkResult enabledSourceResult = runCanvasLiveStrokeWork(dryRequest);
    if (dryResult.samples.size() != enabledSourceResult.samples.size()
            || brushNeedsSourceLayer(dryRequest.brush)) {
        return 1;
    }

    StrokeInput denseStroke;
    denseStroke.points = {
            {{0.0, 8.0}, 1.0, 0.0},
            {{120.0, 8.0}, 1.0, 1.0},
    };

    CanvasLiveStrokeWorkRequest denseLiveRequest;
    denseLiveRequest.rawInput = denseStroke;
    denseLiveRequest.brush = makeBrush();
    denseLiveRequest.brush.rasterizer.brushSize = 24.0;
    denseLiveRequest.brush.rasterizer.radius = 12;
    denseLiveRequest.brush.rasterizer.spacing = 0.0;
    denseLiveRequest.brush.rasterizer.spacingRatio = 0.0;
    denseLiveRequest.stabilizer = Stabilizer{0.0};
    denseLiveRequest.projection = makeProjection();
    denseLiveRequest.sourceLayerEnabled = false;

    CanvasCommitStrokeWorkRequest denseCommitRequest;
    denseCommitRequest.rawInput = denseStroke;
    denseCommitRequest.brush = denseLiveRequest.brush;
    denseCommitRequest.stabilizer = denseLiveRequest.stabilizer;
    denseCommitRequest.projection = denseLiveRequest.projection;
    denseCommitRequest.sourceLayerEnabled = false;

    const CanvasLiveStrokeWorkResult denseLiveResult = runCanvasLiveStrokeWork(denseLiveRequest);
    const CanvasCommitStrokeWorkResult denseCommitResult = runCanvasCommitStrokeWork(denseCommitRequest);
    if (denseLiveResult.frame.dabs.empty()
            || denseCommitResult.command.dabs.size() <= denseLiveResult.frame.dabs.size()
            || denseLiveResult.samples.empty()
            || denseCommitResult.samples.empty()
            || isEmpty(denseLiveResult.fullDirtyBounds)
            || isEmpty(denseLiveResult.dirtyBounds)
            || isEmpty(denseCommitResult.dirtyBounds)) {
        return 1;
    }

    CanvasLiveStrokeWorkRequest incrementalLiveRequest = denseLiveRequest;
    incrementalLiveRequest.incrementalPreviewEnabled = true;
    incrementalLiveRequest.incrementalPreviewStartDistance = 60.0;
    const CanvasLiveStrokeWorkResult incrementalLiveResult = runCanvasLiveStrokeWork(incrementalLiveRequest);
    if (!incrementalLiveResult.incrementalPreview
            || incrementalLiveResult.incrementalPreviewStartDistance != 60.0
            || incrementalLiveResult.samples.empty()
            || incrementalLiveResult.samples.size() >= denseLiveResult.samples.size()
            || isEmpty(incrementalLiveResult.dirtyBounds)
            || isEmpty(incrementalLiveResult.fullDirtyBounds)
            || incrementalLiveResult.fullDirtyBounds.width <= incrementalLiveResult.dirtyBounds.width
            || incrementalLiveResult.renderedStrokeDistance <= incrementalLiveRequest.incrementalPreviewStartDistance) {
        return 1;
    }

    CanvasCommitStrokeWorkRequest commitRequest;
    commitRequest.rawInput = makeRawStroke();
    commitRequest.brush = makeBrush();
    commitRequest.stabilizer = Stabilizer{0.25};
    commitRequest.projection = makeProjection();
    commitRequest.sourceLayerEnabled = false;

    auto commitFuture = std::async(std::launch::async, [mainThread, commitRequest]() {
        const CanvasCommitStrokeWorkResult result = runCanvasCommitStrokeWork(commitRequest);
        return std::pair{std::this_thread::get_id() != mainThread, result};
    });
    const auto [commitRanOffGuiThread, commitResult] = commitFuture.get();
    if (!commitRanOffGuiThread
            || commitResult.command.path.rawInput.points.size() != commitRequest.rawInput.points.size()
            || commitResult.command.dabs.empty()
            || commitResult.samples.empty()
            || isEmpty(commitResult.dirtyBounds)) {
        return 1;
    }

    RasterLayer layer = makeRasterLayer(24, 16);
    paintRasterSamples(layer, commitResult.samples);
    bool painted = false;
    for (const std::uint32_t pixel : layer.pixels) {
        painted = painted || alphaOf(pixel) > 0;
    }

    return painted ? 0 : 1;
}
