//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include "Core/PaintPoint.h"
#include "Filter/FilterPipeline.h"
#include "Selection/Selection.h"
#include "Tool/RasterEditTool.h"
#include "Transform/Transform.h"

enum class ToolKind {
    Brush,
    Selection,
    Transform,
    Crop,
    Fill,
    Gradient,
    Eraser,
    Blur,
    Smudge,
};

enum class ToolPhase {
    Idle,
    Dragging,
    Committed,
    Cancelled,
};

struct ToolState {
    ToolKind activeTool = ToolKind::Brush;
    ToolPhase phase = ToolPhase::Idle;
    DocumentPoint startPoint{};
    DocumentPoint currentPoint{};
    SelectionState selection;
    AffineTransform transform;
    FillOperation fill;
    GradientOperation gradient;
    EraserOperation eraser;
    FilterPipeline filterPipeline;
};

struct ToolStateMachine {
    bool enabled = true;
    ToolState state;
};

void beginTool(ToolStateMachine &machine, ToolKind tool, DocumentPoint startPoint);

void updateToolDrag(ToolStateMachine &machine, DocumentPoint currentPoint);

void commitTool(ToolStateMachine &machine);

void cancelTool(ToolStateMachine &machine);
