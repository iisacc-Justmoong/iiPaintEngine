//
// Created by Justmoong on 2026 May 24.
//

#include "DirtyRegion.h"

#include <algorithm>

bool isEmpty(DevicePixelRect rect)
{
    return rect.width <= 0 || rect.height <= 0;
}

DevicePixelRect uniteDevicePixelRects(DevicePixelRect lhs, DevicePixelRect rhs)
{
    if (isEmpty(lhs)) {
        return rhs;
    }
    if (isEmpty(rhs)) {
        return lhs;
    }

    const Types::Pixel left = std::min(lhs.origin.x, rhs.origin.x);
    const Types::Pixel top = std::min(lhs.origin.y, rhs.origin.y);
    const Types::Pixel right = std::max(lhs.origin.x + lhs.width, rhs.origin.x + rhs.width);
    const Types::Pixel bottom = std::max(lhs.origin.y + lhs.height, rhs.origin.y + rhs.height);
    return DevicePixelRect{
            {left, top},
            std::max<Types::Pixel>(0, right - left),
            std::max<Types::Pixel>(0, bottom - top),
    };
}

DevicePixelRect intersectDevicePixelRects(DevicePixelRect lhs, DevicePixelRect rhs)
{
    if (isEmpty(lhs) || isEmpty(rhs)) {
        return {};
    }

    const Types::Pixel left = std::max(lhs.origin.x, rhs.origin.x);
    const Types::Pixel top = std::max(lhs.origin.y, rhs.origin.y);
    const Types::Pixel right = std::min(lhs.origin.x + lhs.width, rhs.origin.x + rhs.width);
    const Types::Pixel bottom = std::min(lhs.origin.y + lhs.height, rhs.origin.y + rhs.height);
    return DevicePixelRect{
            {left, top},
            std::max<Types::Pixel>(0, right - left),
            std::max<Types::Pixel>(0, bottom - top),
    };
}

DirtyRegion makeDirtyRegion(const std::vector<DevicePixelRect> &rects)
{
    DirtyRegion region;
    region.rects.reserve(rects.size());
    for (const DevicePixelRect rect : rects) {
        if (isEmpty(rect)) {
            continue;
        }

        region.rects.push_back(rect);
        region.bounds = uniteDevicePixelRects(region.bounds, rect);
    }

    return region;
}
