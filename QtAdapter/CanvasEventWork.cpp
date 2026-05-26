//
// Created by Justmoong on 2026 May 25.
//

#include "CanvasEventWork.h"

#include "Render/DirtyRegion.h"

#include <cstdint>

namespace {

std::uint32_t sampleRasterLayerArgb(const void *context, DevicePixelPoint position)
{
    const auto *layer = static_cast<const RasterLayer *>(context);
    if (layer == nullptr) {
        return 0x00000000U;
    }

    return rasterLayerPixelAt(*layer, position);
}

RasterSourceSampler sourceSamplerForLayer(const RasterLayer &layer)
{
    RasterSourceSampler sampler;
    sampler.context = &layer;
    sampler.sampleArgb = &sampleRasterLayerArgb;
    sampler.width = layer.width;
    sampler.height = layer.height;
    return sampler;
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
    result.frame = makeLiveStrokeFrame(request.rawInput, request.brush, request.stabilizer, false);
    if (!result.frame.active) {
        return result;
    }

    if (request.sourceLayerEnabled && brushNeedsSourceLayer(request.brush)) {
        const RasterSourceSampler sourceSampler = sourceSamplerForLayer(request.sourceLayer);
        result.samples = projectBrushDabs(result.frame.dabs,
                                          request.brush.rasterizer,
                                          request.projection,
                                          sourceSampler,
                                          request.brush.material);
    } else {
        result.samples = projectBrushDabs(result.frame.dabs,
                                          request.brush.rasterizer,
                                          request.projection,
                                          request.brush.material);
    }
    result.dirtyBounds = makeDirtyRegion(deviceBoundsForBrushDabs(result.frame.dabs,
                                                                  request.brush.rasterizer,
                                                                  request.projection)).bounds;
    result.frame.dirtyBounds = result.dirtyBounds;
    return result;
}

CanvasCommitStrokeWorkResult runCanvasCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request)
{
    CanvasCommitStrokeWorkResult result;
    result.command = makeStrokeCommand(request.rawInput, request.brush, request.stabilizer);
    if (request.sourceLayerEnabled && brushNeedsSourceLayer(result.command.brush)) {
        const RasterSourceSampler sourceSampler = sourceSamplerForLayer(request.sourceLayer);
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
    result.dirtyBounds = makeDirtyRegion(deviceBoundsForBrushDabs(result.command.dabs,
                                                                  result.command.brush.rasterizer,
                                                                  request.projection)).bounds;
    return result;
}
