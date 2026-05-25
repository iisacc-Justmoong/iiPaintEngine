//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "Core/PaintRect.h"
#include "Core/Types.h"

struct CanvasViewport {
    DocumentRect documentRect;
    ViewRect viewRect;
    DevicePixelRect devicePixelRect;
    Types::Scalar zoom = 1.0;
    Types::Scalar devicePixelRatio = 1.0;
};

DocumentPoint documentPointFromViewPoint(const CanvasViewport &viewport, ViewPoint point);

ViewPoint viewPointFromDocumentPoint(const CanvasViewport &viewport, DocumentPoint point);

DevicePixelPoint devicePixelPointFromViewPoint(const CanvasViewport &viewport, ViewPoint point);

DevicePixelPoint devicePixelPointFromDocumentPoint(const CanvasViewport &viewport, DocumentPoint point);

DevicePixelRect devicePixelRectFromDocumentRect(const CanvasViewport &viewport, DocumentRect rect);
