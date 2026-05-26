//
// Created by Justmoong on 2026 May 26.
//

#include "ToolStateMachine.h"

#include <algorithm>

namespace {

DocumentRect rectFromDrag(DocumentPoint start, DocumentPoint end)
{
    const Types::Scalar left = std::min(start.x, end.x);
    const Types::Scalar top = std::min(start.y, end.y);
    const Types::Scalar right = std::max(start.x, end.x);
    const Types::Scalar bottom = std::max(start.y, end.y);
    return {{left, top}, right - left, bottom - top};
}

} // namespace

void beginTool(ToolStateMachine &machine, ToolKind tool, DocumentPoint startPoint)
{
    machine.state.activeTool = tool;
    machine.state.phase = ToolPhase::Dragging;
    machine.state.startPoint = startPoint;
    machine.state.currentPoint = startPoint;
    machine.state.transform = AffineTransform{};
}

void updateToolDrag(ToolStateMachine &machine, DocumentPoint currentPoint)
{
    machine.state.currentPoint = currentPoint;
    if (machine.state.activeTool == ToolKind::Selection) {
        machine.state.selection = makeRectangularSelection(rectFromDrag(machine.state.startPoint, currentPoint));
    } else if (machine.state.activeTool == ToolKind::Transform) {
        machine.state.transform = makeTranslationTransform(currentPoint.x - machine.state.startPoint.x,
                                                           currentPoint.y - machine.state.startPoint.y);
    } else if (machine.state.activeTool == ToolKind::Gradient) {
        machine.state.gradient.start = machine.state.startPoint;
        machine.state.gradient.end = currentPoint;
    }
}

void commitTool(ToolStateMachine &machine)
{
    machine.state.phase = ToolPhase::Committed;
}

void cancelTool(ToolStateMachine &machine)
{
    machine.state.phase = ToolPhase::Cancelled;
}
