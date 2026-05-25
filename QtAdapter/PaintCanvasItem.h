//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QQuickPaintedItem>

#include "Canvas/CanvasViewport.h"
#include "Input/InputStrokeBuilder.h"
#include "Layer/RasterLayer.h"
#include "Render/DirtyRegion.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeInput.h"

class QMouseEvent;
class QPainter;

class PaintCanvasItem : public QQuickPaintedItem {
    Q_OBJECT

public:
    explicit PaintCanvasItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void setDocumentViewport(qreal documentX, qreal documentY, qreal zoom);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void ensureRasterLayerSize();
    void updateViewportGeometry();
    void handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase);
    PointerEvent makeDocumentPointerEvent(QMouseEvent *event, PointerEventPhase phase) const;
    RasterProjection currentRasterProjection() const;
    DevicePixelRect layerBounds() const;
    void requestTextureUpdate(DevicePixelRect dirtyBounds);
    void updateLiveStrokePreview();
    void clearLiveStrokePreview();
    void commitStroke(const StrokeInput &stroke);
    BrushState currentBrushState() const;

    RasterLayer m_rasterLayer;
    RasterLayer m_liveRasterLayer;
    InputStrokeBuilder m_strokeBuilder;
    LiveStrokeBuffer m_liveStrokeBuffer;
    Stabilizer m_stabilizer{0.25};
    Rasterizer m_rasterizer{};
    CanvasViewport m_viewport{};
    DocumentPoint m_documentOrigin{};
    Types::Scalar m_zoom = 1.0;
    DevicePixelRect m_liveStrokeDeviceDirtyBounds{};
    std::uint32_t m_nextStrokeSeed = 1;
};
