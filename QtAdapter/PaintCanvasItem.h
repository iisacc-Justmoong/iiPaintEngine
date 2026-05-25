//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QThreadPool>

#include <cstdint>

#include "Canvas/CanvasViewport.h"
#include "Input/InputStrokeBuilder.h"
#include "Layer/RasterLayer.h"
#include "QtAdapter/CanvasEventWork.h"
#include "Render/DirtyRegion.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/Stabilizer.h"
#include "Stroke/StrokeInput.h"

class QMouseEvent;
class QPainter;

class PaintCanvasItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(qreal documentX READ documentX WRITE setDocumentX NOTIFY viewportChanged)
    Q_PROPERTY(qreal documentY READ documentY WRITE setDocumentY NOTIFY viewportChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewportChanged)
    Q_PROPERTY(qreal canvasDevicePixelRatio READ canvasDevicePixelRatio WRITE setCanvasDevicePixelRatio NOTIFY viewportChanged)
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushChanged)
    Q_PROPERTY(qreal brushSpacing READ brushSpacing WRITE setBrushSpacing NOTIFY brushChanged)
    Q_PROPERTY(qreal brushSpacingRatio READ brushSpacingRatio WRITE setBrushSpacingRatio NOTIFY brushChanged)
    Q_PROPERTY(qreal brushFlow READ brushFlow WRITE setBrushFlow NOTIFY brushChanged)
    Q_PROPERTY(qreal brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY brushChanged)
    Q_PROPERTY(qreal brushHardness READ brushHardness WRITE setBrushHardness NOTIFY brushChanged)
    Q_PROPERTY(bool livePreviewEnabled READ livePreviewEnabled WRITE setLivePreviewEnabled NOTIFY livePreviewEnabledChanged)
    Q_PROPERTY(bool multithreadedEventsEnabled READ multithreadedEventsEnabled WRITE setMultithreadedEventsEnabled NOTIFY multithreadedEventsEnabledChanged)
    Q_PROPERTY(bool liveStrokeActive READ liveStrokeActive NOTIFY liveStrokeActiveChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokeCountChanged)

public:
    explicit PaintCanvasItem(QQuickItem *parent = nullptr);
    ~PaintCanvasItem() override;

    void paint(QPainter *painter) override;

    qreal documentX() const;
    void setDocumentX(qreal value);

    qreal documentY() const;
    void setDocumentY(qreal value);

    qreal zoom() const;
    void setZoom(qreal value);

    qreal canvasDevicePixelRatio() const;
    void setCanvasDevicePixelRatio(qreal value);

    QColor brushColor() const;
    void setBrushColor(const QColor &color);

    qreal brushSize() const;
    void setBrushSize(qreal value);

    qreal brushSpacing() const;
    void setBrushSpacing(qreal value);

    qreal brushSpacingRatio() const;
    void setBrushSpacingRatio(qreal value);

    qreal brushFlow() const;
    void setBrushFlow(qreal value);

    qreal brushOpacity() const;
    void setBrushOpacity(qreal value);

    qreal brushHardness() const;
    void setBrushHardness(qreal value);

    bool livePreviewEnabled() const;
    void setLivePreviewEnabled(bool enabled);

    bool multithreadedEventsEnabled() const;
    void setMultithreadedEventsEnabled(bool enabled);

    bool liveStrokeActive() const;
    int strokeCount() const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void setDocumentViewport(qreal documentX, qreal documentY, qreal zoom);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void panBy(qreal documentDx, qreal documentDy);
    Q_INVOKABLE void zoomAt(qreal viewX, qreal viewY, qreal factor);
    Q_INVOKABLE void setBrush(qreal size, const QColor &color, qreal flow, qreal opacity);

signals:
    void viewportChanged();
    void brushChanged();
    void livePreviewEnabledChanged();
    void multithreadedEventsEnabledChanged();
    void liveStrokeActiveChanged();
    void strokeCountChanged();

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
    void applyLiveStrokeWorkResult(std::uint64_t generation, const CanvasLiveStrokeWorkResult &result);
    void clearLiveStrokePreview();
    void commitStroke(const StrokeInput &stroke);
    void applyCommitStrokeWorkResult(std::uint64_t revision, const CanvasCommitStrokeWorkResult &result);
    void emitLiveStrokeActiveChangedIfNeeded(bool previousActive);
    BrushState currentBrushState() const;
    CanvasLiveStrokeWorkRequest currentLiveStrokeWorkRequest() const;
    CanvasCommitStrokeWorkRequest currentCommitStrokeWorkRequest(const StrokeInput &stroke) const;
    void invalidatePendingCanvasEventWork();

    RasterLayer m_rasterLayer;
    RasterLayer m_liveRasterLayer;
    QThreadPool m_liveEventThreadPool;
    QThreadPool m_commitEventThreadPool;
    InputStrokeBuilder m_strokeBuilder;
    LiveStrokeBuffer m_liveStrokeBuffer;
    Stabilizer m_stabilizer{0.25};
    Rasterizer m_rasterizer{};
    CanvasViewport m_viewport{};
    DocumentPoint m_documentOrigin{};
    Types::Scalar m_zoom = 1.0;
    Types::Scalar m_devicePixelRatio = 1.0;
    DevicePixelRect m_liveStrokeDeviceDirtyBounds{};
    bool m_livePreviewEnabled = true;
    bool m_multithreadedEventsEnabled = true;
    std::uint64_t m_canvasEventRevision = 0;
    std::uint64_t m_livePreviewGeneration = 0;
    std::uint32_t m_nextStrokeSeed = 1;
};
