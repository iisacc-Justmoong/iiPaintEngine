//
// Created by Justmoong on 2026 May 25.
//

#include "CanvasEventWork.h"

#include "Render/DirtyRegion.h"

CanvasLiveStrokeWorkResult runCanvasLiveStrokeWork(const CanvasLiveStrokeWorkRequest &request)
{
    CanvasLiveStrokeWorkResult result;
    result.frame = makeLiveStrokeFrame(request.rawInput, request.brush, request.stabilizer);
    if (!result.frame.active) {
        return result;
    }

    result.samples = projectBrushDabs(result.frame.dabs,
                                      request.brush.rasterizer,
                                      request.projection);
    result.dirtyBounds = makeDirtyRegion(deviceBoundsForBrushDabs(result.frame.dabs,
                                                                  request.brush.rasterizer,
                                                                  request.projection)).bounds;
    result.frame.samples = result.samples;
    result.frame.dirtyBounds = result.dirtyBounds;
    return result;
}

CanvasCommitStrokeWorkResult runCanvasCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request)
{
    CanvasCommitStrokeWorkResult result;
    result.command = makeStrokeCommand(request.rawInput, request.brush, request.stabilizer);
    result.samples = projectBrushDabs(result.command.dabs,
                                      result.command.brush.rasterizer,
                                      request.projection);
    result.dirtyBounds = makeDirtyRegion(deviceBoundsForBrushDabs(result.command.dabs,
                                                                  result.command.brush.rasterizer,
                                                                  request.projection)).bounds;
    return result;
}
