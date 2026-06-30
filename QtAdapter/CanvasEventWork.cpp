//
// Created by Justmoong on 2026 May 25.
//

#include "CanvasEventWork.h"

#include "Render/DirtyRegion.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace {

constexpr Types::Scalar minimumLivePreviewDabSpacing = 1.0;

std::uint32_t sampleRasterLayerArgb(const void *context, DevicePixelPoint position)
{
    const auto *layer = static_cast<const RasterLayer *>(context);
    if (layer == nullptr) {
        return 0x00000000U;
    }

    return rasterLayerPixelAt(*layer, position);
}

RasterSourceSampler sourceSamplerForLayer(const RasterLayer &layer, DevicePixelPoint origin)
{
    RasterSourceSampler sampler;
    sampler.context = &layer;
    sampler.sampleArgb = &sampleRasterLayerArgb;
    sampler.origin = origin;
    sampler.width = layer.width;
    sampler.height = layer.height;
    return sampler;
}

BrushState livePreviewBrushState(const BrushState &brush)
{
    BrushState previewBrush = brush;
    if (!previewBrush.rasterizer.spacingEnabled) {
        return previewBrush;
    }

    if (previewBrush.rasterizer.brushSize > 0.0) {
        const Types::Scalar minimumRatio = std::min<Types::Scalar>(
                1.0,
                minimumLivePreviewDabSpacing / previewBrush.rasterizer.brushSize);
        previewBrush.rasterizer.spacingRatio = std::max(previewBrush.rasterizer.spacingRatio, minimumRatio);
    } else {
        previewBrush.rasterizer.spacing = std::max(previewBrush.rasterizer.spacing,
                                                   minimumLivePreviewDabSpacing);
    }
    return previewBrush;
}

std::span<const BrushDab> brushDabsFromDistance(const std::vector<BrushDab> &dabs,
                                                Types::Scalar startDistance)
{
    const auto firstDab = std::lower_bound(dabs.begin(),
                                           dabs.end(),
                                           startDistance,
                                           [](const BrushDab &dab, Types::Scalar distance) {
                                               return dab.strokeDistance < distance;
                                           });
    if (firstDab == dabs.end()) {
        return {};
    }

    return std::span<const BrushDab>{&*firstDab, static_cast<std::size_t>(dabs.end() - firstDab)};
}

} // namespace

bool brushNeedsSourceLayer(const BrushState &brush)
{
    return brush.material.simulation.enabled
            && brush.material.simulation.model != BrushSimulationModel::Dry;
}

CanvasLiveStrokeWorkResult runCanvasLiveStrokeWork(const CanvasLiveStrokeWorkRequest &request)
{
    CanvasLiveStrokeWorkResult result;
    const BrushState previewBrush = livePreviewBrushState(request.brush);
    result.frame = makeLiveStrokeFrame(request.rawInput, previewBrush, request.stabilizer, false);
    if (!result.frame.active) {
        return result;
    }

    result.fullDirtyBounds = deviceBoundsForBrushDabsUnion(result.frame.dabs,
                                                           previewBrush.rasterizer,
                                                           request.projection);
    result.frame.dirtyBounds = result.fullDirtyBounds;
    if (!result.frame.dabs.empty()) {
        result.renderedStrokeDistance = result.frame.dabs.back().strokeDistance;
    }

    const bool canProjectIncrementally = request.incrementalPreviewEnabled
            && request.incrementalPreviewStartDistance > 0.0
            && !result.frame.dabs.empty();
    const std::span<const BrushDab> incrementalDabs = canProjectIncrementally
            ? brushDabsFromDistance(result.frame.dabs, request.incrementalPreviewStartDistance)
            : std::span<const BrushDab>{};
    const std::span<const BrushDab> projectedDabs = canProjectIncrementally
            ? incrementalDabs
            : std::span<const BrushDab>{result.frame.dabs.data(), result.frame.dabs.size()};
    result.incrementalPreview = canProjectIncrementally;
    result.incrementalPreviewStartDistance = canProjectIncrementally
            ? request.incrementalPreviewStartDistance
            : 0.0;

    if (request.sourceLayerEnabled && brushNeedsSourceLayer(previewBrush)) {
        const RasterSourceSampler sourceSampler = sourceSamplerForLayer(request.sourceLayer,
                                                                       request.sourceLayerOrigin);
        result.samples = projectBrushDabs(projectedDabs,
                                          previewBrush.rasterizer,
                                          request.projection,
                                          sourceSampler,
                                          previewBrush.material);
    } else {
        result.samples = projectBrushDabs(projectedDabs,
                                          previewBrush.rasterizer,
                                          request.projection,
                                          previewBrush.material);
    }
    result.dirtyBounds = deviceBoundsForBrushDabsUnion(projectedDabs,
                                                       previewBrush.rasterizer,
                                                       request.projection);
    return result;
}

CanvasCommitStrokeWorkResult runCanvasCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request)
{
    CanvasCommitStrokeWorkResult result;
    result.command = makeStrokeCommand(request.rawInput, request.brush, request.stabilizer);
    if (request.sourceLayerEnabled && brushNeedsSourceLayer(result.command.brush)) {
        const RasterSourceSampler sourceSampler = sourceSamplerForLayer(request.sourceLayer,
                                                                       request.sourceLayerOrigin);
        result.samples = projectBrushDabs(result.command.dabs,
                                          result.command.brush.rasterizer,
                                          request.projection,
                                          sourceSampler,
                                          result.command.brush.material);
    } else {
        result.samples = projectBrushDabs(result.command.dabs,
                                          result.command.brush.rasterizer,
                                          request.projection,
                                          result.command.brush.material);
    }
    result.dirtyBounds = deviceBoundsForBrushDabsUnion(result.command.dabs,
                                                       result.command.brush.rasterizer,
                                                       request.projection);
    return result;
}
