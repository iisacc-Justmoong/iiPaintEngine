//
// Created by Justmoong on 2026 May 24.
//

#include "BrushSnapshot.h"

BrushSnapshot snapshotBrushPreset(const BrushPreset &p)
{
    return {p.brushId, p.name, p.tip, p.size, p.opacity, p.hardness, p.flow, p.density,
            p.dynamics, p.material, p.shape, p.stroke, p.tipSequence, p.color};
}

BrushPreset restoreBrushPreset(const BrushSnapshot &p)
{
    return {p.brushId, p.name, p.tip, p.size, p.opacity, p.hardness, p.flow, p.density,
            p.dynamics, p.material, p.shape, p.stroke, p.tipSequence, p.color};
}
