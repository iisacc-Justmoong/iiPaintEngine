//
// Created by Justmoong on 2026 May 26.
//

#include "Selection.h"

#include <algorithm>
#include <cstddef>

namespace {

bool containsRect(DocumentRect rect, DocumentPoint point)
{
    return point.x >= rect.origin.x
            && point.y >= rect.origin.y
            && point.x < rect.origin.x + rect.width
            && point.y < rect.origin.y + rect.height;
}

bool containsMask(const SelectionMask &mask, DevicePixelPoint point)
{
    return point.x >= mask.bounds.origin.x
            && point.y >= mask.bounds.origin.y
            && point.x < mask.bounds.origin.x + mask.bounds.width
            && point.y < mask.bounds.origin.y + mask.bounds.height;
}

Types::Scalar applyInversion(const SelectionState &selection, Types::Scalar alpha)
{
    return selection.inverted ? 1.0 - alpha : alpha;
}

} // namespace

SelectionState makeRectangularSelection(DocumentRect bounds)
{
    SelectionState selection;
    selection.active = bounds.width > 0.0 && bounds.height > 0.0;
    selection.shape = selection.active ? SelectionShape::Rectangle : SelectionShape::None;
    selection.bounds = bounds;
    return selection;
}

bool selectionContains(const SelectionState &selection, DocumentPoint point)
{
    if (!selection.active) {
        return true;
    }

    const bool contains = containsRect(selection.bounds, point);
    return selection.inverted ? !contains : contains;
}

Types::Scalar selectionAlphaAt(const SelectionState &selection, DevicePixelPoint point)
{
    if (!selection.active) {
        return 1.0;
    }

    Types::Scalar alpha = 0.0;
    if (selection.shape == SelectionShape::Mask && containsMask(selection.mask, point)) {
        const auto x = static_cast<std::size_t>(point.x - selection.mask.bounds.origin.x);
        const auto y = static_cast<std::size_t>(point.y - selection.mask.bounds.origin.y);
        const auto width = static_cast<std::size_t>(std::max<Types::Pixel>(0, selection.mask.bounds.width));
        const std::size_t index = y * width + x;
        if (index < selection.mask.alpha.size()) {
            alpha = static_cast<Types::Scalar>(selection.mask.alpha[index]) / 255.0;
        }
    } else if (selection.shape == SelectionShape::Rectangle
            && selectionContains(selection, {static_cast<Types::Scalar>(point.x),
                                            static_cast<Types::Scalar>(point.y)})) {
        alpha = 1.0;
    }

    return applyInversion(selection, std::clamp(alpha, 0.0, 1.0));
}
