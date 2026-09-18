//
// Created by Justmoong on 2026 May 26.
//

#include "Transform.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

std::size_t surfaceIndex(const DrawingSurface &surface, DevicePixelPoint point)
{
    return static_cast<std::size_t>(point.y) * static_cast<std::size_t>(surface.width)
            + static_cast<std::size_t>(point.x);
}

bool contains(const DrawingSurface &surface, DevicePixelPoint point)
{
    return point.x >= 0 && point.y >= 0 && point.x < surface.width && point.y < surface.height;
}

} // namespace

AffineTransform makeTranslationTransform(Types::Scalar dx, Types::Scalar dy)
{
    AffineTransform transform;
    transform.translationX = dx;
    transform.translationY = dy;
    return transform;
}

DocumentPoint transformPoint(AffineTransform transform, DocumentPoint point)
{
    return {
            point.x * transform.m11 + point.y * transform.m21 + transform.translationX,
            point.x * transform.m12 + point.y * transform.m22 + transform.translationY,
    };
}

DocumentRect transformRect(AffineTransform transform, DocumentRect rect)
{
    const std::array<DocumentPoint, 4> points{
            transformPoint(transform, rect.origin),
            transformPoint(transform, {rect.origin.x + rect.width, rect.origin.y}),
            transformPoint(transform, {rect.origin.x, rect.origin.y + rect.height}),
            transformPoint(transform, {rect.origin.x + rect.width, rect.origin.y + rect.height}),
    };

    Types::Scalar left = points.front().x;
    Types::Scalar top = points.front().y;
    Types::Scalar right = points.front().x;
    Types::Scalar bottom = points.front().y;
    for (const DocumentPoint point : points) {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }

    return {{left, top}, right - left, bottom - top};
}

SelectionState transformSelection(const SelectionState &selection, AffineTransform transform)
{
    SelectionState transformed = selection;
    transformed.bounds = transformRect(transform, selection.bounds);
    if (selection.shape == SelectionShape::Rectangle) {
        transformed.shape = SelectionShape::Rectangle;
    }
    return transformed;
}

DrawingSurface cropDrawingSurface(const DrawingSurface &surface, DevicePixelRect cropRect)
{
    const Types::Pixel left = std::clamp(cropRect.origin.x, 0, surface.width);
    const Types::Pixel top = std::clamp(cropRect.origin.y, 0, surface.height);
    const Types::Pixel right = std::clamp(cropRect.origin.x + cropRect.width, 0, surface.width);
    const Types::Pixel bottom = std::clamp(cropRect.origin.y + cropRect.height, 0, surface.height);
    DrawingSurface cropped = makeDrawingSurface(std::max<Types::Pixel>(0, right - left),
                                                std::max<Types::Pixel>(0, bottom - top));

    for (Types::Pixel y = 0; y < cropped.height; ++y) {
        for (Types::Pixel x = 0; x < cropped.width; ++x) {
            const DevicePixelPoint sourcePoint{left + x, top + y};
            const DevicePixelPoint targetPoint{x, y};
            if (!contains(surface, sourcePoint)) {
                continue;
            }
            cropped.pixels[surfaceIndex(cropped, targetPoint)] = surface.pixels[surfaceIndex(surface, sourcePoint)];
        }
    }
    cropped.dirtyBounds = {{0, 0}, cropped.width, cropped.height};
    return cropped;
}
