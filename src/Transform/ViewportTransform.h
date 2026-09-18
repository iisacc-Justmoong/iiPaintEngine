#pragma once

#include "Core/PaintRect.h"
#include "Core/Types.h"

struct RasterViewport {
    DocumentRect documentRect;
    ViewRect viewRect;
    DevicePixelRect devicePixelRect;
    Types::Scalar zoom = 1.0;
    Types::Scalar devicePixelRatio = 1.0;
};

DocumentPoint documentPointFromViewPoint(const RasterViewport &viewport, ViewPoint point);

ViewPoint viewPointFromDocumentPoint(const RasterViewport &viewport, DocumentPoint point);

DevicePixelPoint devicePixelPointFromViewPoint(const RasterViewport &viewport, ViewPoint point);

DevicePixelPoint devicePixelPointFromDocumentPoint(const RasterViewport &viewport, DocumentPoint point);

DevicePixelRect devicePixelRectFromDocumentRect(const RasterViewport &viewport, DocumentRect rect);
