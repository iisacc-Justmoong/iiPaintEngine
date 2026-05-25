//
// Created by Justmoong on 2026 May 24.
//

#include "CanvasViewport.h"

#include <algorithm>
#include <cmath>

namespace {

Types::Scalar safeZoom(const CanvasViewport &viewport)
{
    return std::max<Types::Scalar>(0.01, viewport.zoom);
}

Types::Scalar safeDevicePixelRatio(const CanvasViewport &viewport)
{
    return std::max<Types::Scalar>(0.01, viewport.devicePixelRatio);
}

Types::Pixel floorPixel(Types::Scalar value)
{
    return static_cast<Types::Pixel>(std::floor(value));
}

Types::Pixel ceilPixel(Types::Scalar value)
{
    return static_cast<Types::Pixel>(std::ceil(value));
}

} // namespace

DocumentPoint documentPointFromViewPoint(const CanvasViewport &viewport, ViewPoint point)
{
    const Types::Scalar zoom = safeZoom(viewport);
    return DocumentPoint{
            viewport.documentRect.origin.x + (point.x - viewport.viewRect.origin.x) / zoom,
            viewport.documentRect.origin.y + (point.y - viewport.viewRect.origin.y) / zoom,
    };
}

ViewPoint viewPointFromDocumentPoint(const CanvasViewport &viewport, DocumentPoint point)
{
    const Types::Scalar zoom = safeZoom(viewport);
    return ViewPoint{
            viewport.viewRect.origin.x + (point.x - viewport.documentRect.origin.x) * zoom,
            viewport.viewRect.origin.y + (point.y - viewport.documentRect.origin.y) * zoom,
    };
}

DevicePixelPoint devicePixelPointFromViewPoint(const CanvasViewport &viewport, ViewPoint point)
{
    const Types::Scalar ratio = safeDevicePixelRatio(viewport);
    return DevicePixelPoint{
            viewport.devicePixelRect.origin.x
                    + static_cast<Types::Pixel>(std::lround((point.x - viewport.viewRect.origin.x) * ratio)),
            viewport.devicePixelRect.origin.y
                    + static_cast<Types::Pixel>(std::lround((point.y - viewport.viewRect.origin.y) * ratio)),
    };
}

DevicePixelPoint devicePixelPointFromDocumentPoint(const CanvasViewport &viewport, DocumentPoint point)
{
    return devicePixelPointFromViewPoint(viewport, viewPointFromDocumentPoint(viewport, point));
}

DevicePixelRect devicePixelRectFromDocumentRect(const CanvasViewport &viewport, DocumentRect rect)
{
    const ViewPoint viewOrigin = viewPointFromDocumentPoint(viewport, rect.origin);
    const Types::Scalar scale = safeZoom(viewport) * safeDevicePixelRatio(viewport);
    const Types::Scalar left = viewport.devicePixelRect.origin.x + (viewOrigin.x - viewport.viewRect.origin.x)
            * safeDevicePixelRatio(viewport);
    const Types::Scalar top = viewport.devicePixelRect.origin.y + (viewOrigin.y - viewport.viewRect.origin.y)
            * safeDevicePixelRatio(viewport);
    const Types::Scalar right = left + rect.width * scale;
    const Types::Scalar bottom = top + rect.height * scale;

    const Types::Pixel deviceLeft = floorPixel(left);
    const Types::Pixel deviceTop = floorPixel(top);
    const Types::Pixel deviceRight = ceilPixel(right);
    const Types::Pixel deviceBottom = ceilPixel(bottom);
    return DevicePixelRect{
            {deviceLeft, deviceTop},
            std::max<Types::Pixel>(0, deviceRight - deviceLeft),
            std::max<Types::Pixel>(0, deviceBottom - deviceTop),
    };
}
