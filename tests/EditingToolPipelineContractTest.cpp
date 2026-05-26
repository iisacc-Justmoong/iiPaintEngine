#include <cstdint>

#include "Filter/FilterPipeline.h"
#include "Layer/DrawingSurface.h"
#include "Selection/Selection.h"
#include "Tool/RasterEditTool.h"
#include "Tool/ToolStateMachine.h"
#include "Transform/Transform.h"

namespace {

std::uint8_t alphaOf(std::uint32_t argb)
{
    return static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
}

} // namespace

int main()
{
    static_assert(SelectionShape::Rectangle != SelectionShape::Mask);
    static_assert(ToolKind::Selection != ToolKind::Transform);
    static_assert(FilterKind::Blur != FilterKind::Smudge);
    static_assert(TransformInterpolation::Nearest != TransformInterpolation::Bilinear);

    DrawingSurface surface = makeDrawingSurface(5, 4, 0xFF000000U);
    SelectionState selection = makeRectangularSelection({{1.0, 1.0}, 3.0, 2.0});
    if (!selection.active
            || !selectionContains(selection, {1.0, 1.0})
            || !selectionContains(selection, {3.9, 2.9})
            || selectionContains(selection, {4.0, 2.0})) {
        return 1;
    }

    FillOperation fill;
    fill.argb = 0xFFFF0000U;
    applyFill(surface, selection, fill);
    if (drawingSurfacePixelAt(surface, {1, 1}) != 0xFFFF0000U
            || drawingSurfacePixelAt(surface, {0, 0}) != 0xFF000000U) {
        return 1;
    }
    FillOperation disabledFill = fill;
    disabledFill.enabled = false;
    disabledFill.argb = 0xFF00FF00U;
    applyFill(surface, selection, disabledFill);
    if (drawingSurfacePixelAt(surface, {1, 1}) != 0xFFFF0000U) {
        return 1;
    }

    GradientOperation gradient;
    gradient.kind = GradientKind::Linear;
    gradient.start = {1.0, 1.0};
    gradient.end = {3.0, 1.0};
    gradient.startArgb = 0xFFFF0000U;
    gradient.endArgb = 0xFF0000FFU;
    applyGradient(surface, selection, gradient);
    if (drawingSurfacePixelAt(surface, {1, 1}) != 0xFFFF0000U
            || drawingSurfacePixelAt(surface, {2, 1}) != 0xFF800080U
            || drawingSurfacePixelAt(surface, {3, 1}) != 0xFF0000FFU
            || drawingSurfacePixelAt(surface, {4, 1}) != 0xFF000000U) {
        return 1;
    }

    EraserOperation eraser;
    eraser.opacity = 1.0;
    applyEraser(surface, selection, eraser);
    if (alphaOf(drawingSurfacePixelAt(surface, {2, 1})) != 0
            || drawingSurfacePixelAt(surface, {0, 0}) != 0xFF000000U) {
        return 1;
    }

    surface = makeDrawingSurface(5, 4, 0xFF000000U);
    surface.pixels[1 + 1 * surface.width] = 0xFFFFFFFFU;
    surface.pixels[2 + 1 * surface.width] = 0xFF808080U;
    surface.pixels[3 + 1 * surface.width] = 0xFF000000U;
    FilterPipeline pipeline;
    FilterNode blurNode;
    blurNode.kind = FilterKind::Blur;
    blurNode.radius = 1.0;
    pipeline.nodes.push_back(blurNode);
    FilterNode smudgeNode;
    smudgeNode.kind = FilterKind::Smudge;
    smudgeNode.strength = 0.5;
    pipeline.nodes.push_back(smudgeNode);
    applyFilterPipeline(surface, selection, pipeline);
    if (drawingSurfacePixelAt(surface, {2, 1}) == 0xFF808080U
            || drawingSurfacePixelAt(surface, {0, 0}) != 0xFF000000U) {
        return 1;
    }
    const std::uint32_t filteredPixel = drawingSurfacePixelAt(surface, {2, 1});
    FilterPipeline disabledPipeline;
    FilterNode disabledBlur;
    disabledBlur.enabled = false;
    disabledBlur.kind = FilterKind::Blur;
    disabledBlur.radius = 1.0;
    disabledPipeline.nodes.push_back(disabledBlur);
    applyFilterPipeline(surface, selection, disabledPipeline);
    if (drawingSurfacePixelAt(surface, {2, 1}) != filteredPixel) {
        return 1;
    }

    const DrawingSurface cropped = cropDrawingSurface(surface, {{1, 1}, 3, 2});
    if (cropped.width != 3
            || cropped.height != 2
            || drawingSurfacePixelAt(cropped, {0, 0}) != drawingSurfacePixelAt(surface, {1, 1})) {
        return 1;
    }

    const AffineTransform translation = makeTranslationTransform(2.0, -1.0);
    const SelectionState movedSelection = transformSelection(selection, translation);
    if (movedSelection.bounds.origin.x != 3.0
            || movedSelection.bounds.origin.y != 0.0
            || transformPoint(translation, {1.0, 1.0}).x != 3.0) {
        return 1;
    }

    ToolStateMachine tools;
    beginTool(tools, ToolKind::Selection, {1.0, 1.0});
    updateToolDrag(tools, {4.0, 3.0});
    commitTool(tools);
    if (tools.state.activeTool != ToolKind::Selection
            || tools.state.phase != ToolPhase::Committed
            || !tools.state.selection.active
            || tools.state.selection.bounds.width != 3.0) {
        return 1;
    }

    beginTool(tools, ToolKind::Transform, {0.0, 0.0});
    updateToolDrag(tools, {2.0, 1.0});
    cancelTool(tools);
    if (tools.state.phase != ToolPhase::Cancelled
            || tools.state.transform.translationX != 2.0
            || tools.state.transform.translationY != 1.0) {
        return 1;
    }

    ToolStateMachine disabledTools;
    disabledTools.enabled = false;
    beginTool(disabledTools, ToolKind::Selection, {1.0, 1.0});
    updateToolDrag(disabledTools, {4.0, 4.0});
    commitTool(disabledTools);
    if (disabledTools.state.phase != ToolPhase::Idle
            || disabledTools.state.selection.active) {
        return 1;
    }

    return 0;
}
