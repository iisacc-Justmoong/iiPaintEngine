//
// Created by Justmoong on 2026 May 24.
//

#include "StrokeCommand.h"

StrokeCommand makeStrokeCommand(const StrokeInput &rawInput,
                                const BrushState &brush,
                                const Stabilizer &stabilizer)
{
    StrokeCommand command;
    command.brush = brush;
    command.path.rawInput = rawInput;
    command.path.renderedInput = stabilizeStrokeInput(rawInput, stabilizer);
    command.path.renderedInput = resampleStrokeInput(command.path.renderedInput, brush.resampler);
    command.path.renderedCurve = makeStrokeCurve(command.path.renderedInput);
    command.dabs = placeBrushDabs(command.path.renderedCurve,
                                  brush.rasterizer,
                                  brush.dynamics,
                                  brush.material,
                                  brush.randomSeed);
    command.dabDirtyBounds = documentBoundsForEachBrushDab(command.dabs, brush.rasterizer);
    command.dirtyBounds = documentBoundsForBrushDabs(command.dabs, brush.rasterizer);
    return command;
}
