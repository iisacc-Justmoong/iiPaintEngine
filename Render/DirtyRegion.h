//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <vector>

#include "Core/PaintRect.h"

struct DirtyRegion {
    std::vector<DevicePixelRect> rects;
    DevicePixelRect bounds{};
};

bool isEmpty(DevicePixelRect rect);

DevicePixelRect uniteDevicePixelRects(DevicePixelRect lhs, DevicePixelRect rhs);

DevicePixelRect intersectDevicePixelRects(DevicePixelRect lhs, DevicePixelRect rhs);

DirtyRegion makeDirtyRegion(const std::vector<DevicePixelRect> &rects);
