//
// Created by Justmoong on 2026 May 26.
//

#include "Selection.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

bool containsRect(DocumentRect rect, DocumentPoint point)
{
    return point.x >= rect.origin.x
            && point.y >= rect.origin.y
            && point.x < rect.origin.x + rect.width
            && point.y < rect.origin.y + rect.height;
}

bool validRect(DocumentRect rect)
{
    return std::isfinite(rect.origin.x) && std::isfinite(rect.origin.y)
            && std::isfinite(rect.width) && std::isfinite(rect.height)
            && std::isfinite(rect.origin.x + rect.width)
            && std::isfinite(rect.origin.y + rect.height)
            && rect.width > 0 && rect.height > 0;
}

std::int64_t featherRadius(const SelectionState &selection)
{
    if (!std::isfinite(selection.featherRadius) || selection.featherRadius <= 0)
        return 0;
    return static_cast<std::int64_t>(std::min(
            std::ceil(selection.featherRadius),
            static_cast<double>(std::numeric_limits<Types::Pixel>::max())));
}

double maskCoverage(const SelectionMask &mask, DevicePixelPoint point, std::int64_t radius)
{
    if (mask.bounds.width <= 0 || mask.bounds.height <= 0 || mask.alpha.empty())
        return 0;
    // Widen before subtraction/addition; even a mask crossing INT32_MAX is safe.
    const auto x = std::int64_t(point.x) - mask.bounds.origin.x;
    const auto y = std::int64_t(point.y) - mask.bounds.origin.y;
    const auto firstX = std::max<std::int64_t>(0, x - radius);
    const auto lastX = std::min<std::int64_t>(mask.bounds.width - 1, x + radius);
    const auto firstY = std::max<std::int64_t>(0, y - radius);
    const auto lastY = std::min<std::int64_t>(mask.bounds.height - 1, y + radius);
    if (firstX > lastX || firstY > lastY) return 0;
    const auto width = static_cast<std::size_t>(mask.bounds.width);
    double sum = 0;
    for (auto row = firstY; row <= lastY; ++row) {
        const auto rowIndex = static_cast<std::size_t>(row) * width;
        if (rowIndex >= mask.alpha.size()) break;
        const auto availableEnd = std::min<std::int64_t>(lastX,
                static_cast<std::int64_t>(mask.alpha.size() - rowIndex - 1));
        for (auto column = firstX; column <= availableEnd; ++column)
            sum += mask.alpha[rowIndex + static_cast<std::size_t>(column)];
    }
    const double diameter = 2.0 * static_cast<double>(radius) + 1;
    return sum / (255.0 * diameter * diameter);
}

double rectangleCoverage(DocumentRect rect, DevicePixelPoint point, std::int64_t radius)
{
    if (!validRect(rect)) return 0;
    // Count integer samples without looping over a potentially large radius.
    const auto count = [radius](double origin, double extent, Types::Pixel position) {
        const double first = std::max(std::ceil(origin), double(std::int64_t(position) - radius));
        const double end = std::min(std::ceil(origin + extent), double(std::int64_t(position) + radius + 1));
        return std::max(0.0, end - first);
    };
    const double diameter = 2.0 * static_cast<double>(radius) + 1;
    return count(rect.origin.x, rect.width, point.x)
            * count(rect.origin.y, rect.height, point.y) / (diameter * diameter);
}

} // namespace

SelectionState makeRectangularSelection(DocumentRect bounds)
{
    SelectionState selection;
    selection.active = validRect(bounds);
    selection.shape = selection.active ? SelectionShape::Rectangle : SelectionShape::None;
    selection.bounds = bounds;
    return selection;
}

bool selectionContains(const SelectionState &selection, DocumentPoint point)
{
    if (!selection.active) {
        return true;
    }

    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
    if (selection.shape == SelectionShape::Rectangle && featherRadius(selection) == 0) {
        const bool contains = validRect(selection.bounds) && containsRect(selection.bounds, point);
        return selection.inverted ? !contains : contains;
    }
    const double x = std::floor(point.x), y = std::floor(point.y);
    if (x < std::numeric_limits<Types::Pixel>::min() || x > std::numeric_limits<Types::Pixel>::max()
            || y < std::numeric_limits<Types::Pixel>::min() || y > std::numeric_limits<Types::Pixel>::max())
        return false;
    return selectionAlphaAt(selection, {static_cast<Types::Pixel>(x), static_cast<Types::Pixel>(y)}) > 0;
}

Types::Scalar selectionAlphaAt(const SelectionState &selection, DevicePixelPoint point)
{
    if (!selection.active) {
        return 1.0;
    }

    const auto radius = featherRadius(selection);
    double alpha = 0;
    if (selection.shape == SelectionShape::Mask)
        alpha = maskCoverage(selection.mask, point, radius);
    else if (selection.shape == SelectionShape::Rectangle)
        alpha = rectangleCoverage(selection.bounds, point, radius);
    alpha = std::clamp(alpha, 0.0, 1.0);
    return selection.inverted ? 1.0 - alpha : alpha;
}
