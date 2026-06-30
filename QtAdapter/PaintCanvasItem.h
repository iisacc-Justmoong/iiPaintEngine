//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QBasicTimer>
#include <QColor>
#include <QQuickPaintedItem>
#include <QString>
#include <QThreadPool>

#include <deque>
#include <cstdint>
#include <vector>

#include "Canvas/CanvasViewport.h"
#include "Input/InputNormalizer.h"
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
class QEvent;
class QImage;
class QTabletEvent;
class QTimerEvent;

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
    Q_PROPERTY(bool brushSpacingEnabled READ brushSpacingEnabled WRITE setBrushSpacingEnabled NOTIFY brushChanged)
    Q_PROPERTY(qreal brushFlow READ brushFlow WRITE setBrushFlow NOTIFY brushChanged)
    Q_PROPERTY(bool brushFlowEnabled READ brushFlowEnabled WRITE setBrushFlowEnabled NOTIFY brushChanged)
    Q_PROPERTY(qreal brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY brushChanged)
    Q_PROPERTY(bool brushOpacityEnabled READ brushOpacityEnabled WRITE setBrushOpacityEnabled NOTIFY brushChanged)
    Q_PROPERTY(qreal brushHardness READ brushHardness WRITE setBrushHardness NOTIFY brushChanged)
    Q_PROPERTY(bool brushHardnessEnabled READ brushHardnessEnabled WRITE setBrushHardnessEnabled NOTIFY brushChanged)
    Q_PROPERTY(bool eraserMode READ eraserMode WRITE setEraserMode NOTIFY brushChanged)
    Q_PROPERTY(qreal pressureCurveMinimum READ pressureCurveMinimum WRITE setPressureCurveMinimum NOTIFY strokeSettingsChanged)
    Q_PROPERTY(qreal pressureCurveCenter READ pressureCurveCenter WRITE setPressureCurveCenter NOTIFY strokeSettingsChanged)
    Q_PROPERTY(qreal pressureCurveMaximum READ pressureCurveMaximum WRITE setPressureCurveMaximum NOTIFY strokeSettingsChanged)
    Q_PROPERTY(qreal stabilizerStrength READ stabilizerStrength WRITE setStabilizerStrength NOTIFY strokeSettingsChanged)
    Q_PROPERTY(bool livePreviewEnabled READ livePreviewEnabled WRITE setLivePreviewEnabled NOTIFY livePreviewEnabledChanged)
    Q_PROPERTY(int livePreviewFrameIntervalMs READ livePreviewFrameIntervalMs WRITE setLivePreviewFrameIntervalMs NOTIFY livePreviewFrameIntervalMsChanged)
    Q_PROPERTY(bool multithreadedEventsEnabled READ multithreadedEventsEnabled WRITE setMultithreadedEventsEnabled NOTIFY multithreadedEventsEnabledChanged)
    Q_PROPERTY(bool liveStrokeActive READ liveStrokeActive NOTIFY liveStrokeActiveChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokeCountChanged)
    Q_PROPERTY(QString inputDevice READ inputDevice NOTIFY inputStateChanged)
    Q_PROPERTY(qreal inputPressure READ inputPressure NOTIFY inputStateChanged)

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

    bool brushSpacingEnabled() const;
    void setBrushSpacingEnabled(bool enabled);

    qreal brushFlow() const;
    void setBrushFlow(qreal value);

    bool brushFlowEnabled() const;
    void setBrushFlowEnabled(bool enabled);

    qreal brushOpacity() const;
    void setBrushOpacity(qreal value);

    bool brushOpacityEnabled() const;
    void setBrushOpacityEnabled(bool enabled);

    qreal brushHardness() const;
    void setBrushHardness(qreal value);

    bool brushHardnessEnabled() const;
    void setBrushHardnessEnabled(bool enabled);

    bool eraserMode() const;
    void setEraserMode(bool enabled);

    qreal pressureCurveMinimum() const;
    void setPressureCurveMinimum(qreal value);

    qreal pressureCurveCenter() const;
    void setPressureCurveCenter(qreal value);

    qreal pressureCurveMaximum() const;
    void setPressureCurveMaximum(qreal value);

    qreal stabilizerStrength() const;
    void setStabilizerStrength(qreal value);

    bool livePreviewEnabled() const;
    void setLivePreviewEnabled(bool enabled);

    int livePreviewFrameIntervalMs() const;
    void setLivePreviewFrameIntervalMs(int value);

    bool multithreadedEventsEnabled() const;
    void setMultithreadedEventsEnabled(bool enabled);

    bool liveStrokeActive() const;
    int strokeCount() const;
    QString inputDevice() const;
    qreal inputPressure() const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void setDocumentViewport(qreal documentX, qreal documentY, qreal zoom);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void panBy(qreal documentDx, qreal documentDy);
    Q_INVOKABLE void zoomAt(qreal viewX, qreal viewY, qreal factor);
    Q_INVOKABLE void setBrush(qreal size, const QColor &color, qreal flow, qreal opacity);

signals:
    void viewportChanged();
    void brushChanged();
    void strokeSettingsChanged();
    void livePreviewEnabledChanged();
    void livePreviewFrameIntervalMsChanged();
    void multithreadedEventsEnabledChanged();
    void liveStrokeActiveChanged();
    void strokeCountChanged();
    void inputStateChanged();

protected:
    bool event(QEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    bool resetRasterCanvas(Types::Pixel width,
                           Types::Pixel height,
                           std::uint32_t clearArgb = 0x00000000U);
    bool replaceRasterCanvas(const QImage &image);
    bool saveRasterCanvasToFile(const QString &filePath);
    bool undoRasterChange();
    bool redoRasterChange();
    bool canUndoRasterChange() const;
    bool canRedoRasterChange() const;

private:
    struct RasterSnapshot {
        Types::Pixel width = 0;
        Types::Pixel height = 0;
        std::vector<std::uint32_t> pixels;
        std::uint32_t nextStrokeSeed = 1;
        int committedStrokeCount = 0;
    };

    struct RasterPatchSnapshot {
        Types::Pixel width = 0;
        Types::Pixel height = 0;
        DevicePixelRect bounds{};
        std::vector<std::uint32_t> pixels;
        std::uint32_t nextStrokeSeed = 1;
        int committedStrokeCount = 0;
    };

    struct RasterHistoryEntry {
        bool fullCanvas = true;
        RasterSnapshot fullSnapshot;
        RasterPatchSnapshot patchSnapshot;
    };

    void ensureRasterLayerSize();
    void updateViewportGeometry();
    bool shouldIgnoreMousePointerEvent(QMouseEvent *event);
    void handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase);
    PointerEvent makeDocumentPointerEvent(QMouseEvent *event, PointerEventPhase phase) const;
    void handleTabletPointerEvent(QTabletEvent *event, PointerEventPhase phase);
    void noteTabletPointerEvent(const PointerEvent &event);
    TabletState makeTabletState(QTabletEvent *event, PointerEventPhase phase) const;
    PointerEvent makeDocumentPointerEvent(QTabletEvent *event, PointerEventPhase phase) const;
    RasterProjection currentRasterProjection() const;
    DevicePixelRect layerBounds() const;
    void requestTextureUpdate(DevicePixelRect dirtyBounds);
    void requestLiveStrokePreviewFrame();
    void processLiveStrokePreviewFrame();
    void cancelLiveStrokePreviewFrame();
    void updateLiveStrokePreview();
    void startLiveStrokePreviewWork(const CanvasLiveStrokeWorkRequest &request,
                                    std::uint64_t generation,
                                    std::uint64_t revision);
    void startPendingLiveStrokePreviewWork();
    void applyLiveStrokeWorkResult(std::uint64_t generation,
                                   std::uint64_t revision,
                                   const CanvasLiveStrokeWorkResult &result);
    void preserveLiveStrokePreviewForCommit();
    void clearLiveStrokePreview();
    void clearLiveStrokePreviewPixels();
    Types::Scalar liveStrokeIncrementalStartDistance(const BrushState &brush) const;
    DevicePixelRect liveStrokeTailDeviceDirtyBounds(Types::Scalar startDistance) const;
    void enqueueStrokeCommit(const StrokeInput &stroke);
    void requestStrokeCommitFrame();
    void processStrokeCommitFrame();
    void cancelStrokeCommitFrame();
    void startCommitStrokeWork(const CanvasCommitStrokeWorkRequest &request);
    void applyCommitStrokeWorkResult(std::uint64_t revision, const CanvasCommitStrokeWorkResult &result);
    void emitLiveStrokeActiveChangedIfNeeded(bool previousActive);
    void noteInputState(const PointerEvent &event);
    BrushState currentBrushState() const;
    CanvasLiveStrokeWorkRequest currentLiveStrokeWorkRequest() const;
    CanvasCommitStrokeWorkRequest currentCommitStrokeWorkRequest(const StrokeInput &stroke) const;
    void invalidatePendingCanvasEventWork();
    RasterSnapshot captureRasterSnapshot() const;
    RasterPatchSnapshot captureRasterPatchSnapshot(DevicePixelRect dirtyBounds) const;
    RasterHistoryEntry captureRasterHistoryEntry() const;
    RasterHistoryEntry captureRasterHistoryEntry(DevicePixelRect dirtyBounds) const;
    void recordRasterChange();
    void recordRasterChange(DevicePixelRect dirtyBounds);
    void restoreRasterSnapshot(const RasterSnapshot &snapshot);
    void restoreRasterPatchSnapshot(const RasterPatchSnapshot &snapshot);
    void restoreRasterHistoryEntry(const RasterHistoryEntry &entry);

    RasterLayer m_rasterLayer;
    RasterLayer m_liveRasterLayer;
    QThreadPool m_liveEventThreadPool;
    QThreadPool m_commitEventThreadPool;
    QBasicTimer m_livePreviewFrameTimer;
    QBasicTimer m_commitStrokeFrameTimer;
    InputNormalizer m_inputNormalizer;
    InputStrokeBuilder m_strokeBuilder;
    LiveStrokeBuffer m_liveStrokeBuffer;
    Stabilizer m_stabilizer{0.25};
    Rasterizer m_rasterizer{};
    CanvasViewport m_viewport{};
    DocumentPoint m_documentOrigin{};
    CanvasLiveStrokeWorkRequest m_pendingLiveStrokeWorkRequest{};
    std::deque<CanvasCommitStrokeWorkRequest> m_pendingCommitStrokeWorkRequests;
    Types::Scalar m_zoom = 1.0;
    Types::Scalar m_devicePixelRatio = 1.0;
    Types::Scalar m_liveStrokeRenderedDistance = 0.0;
    DevicePixelRect m_liveStrokeDeviceDirtyBounds{};
    Rasterizer m_liveStrokePreviewRasterizer{};
    int m_livePreviewFrameIntervalMs = 8;
    bool m_livePreviewEnabled = true;
    bool m_multithreadedEventsEnabled = true;
    bool m_livePreviewWorkActive = false;
    bool m_livePreviewWorkPending = false;
    bool m_liveStrokePreviewDestinationOut = false;
    bool m_tabletPointerActive = false;
    bool m_suppressMouseAfterTablet = false;
    PointerDeviceKind m_lastInputDevice = PointerDeviceKind::Mouse;
    Types::Scalar m_lastInputPressure = 1.0;
    std::uint64_t m_canvasEventRevision = 0;
    std::uint64_t m_livePreviewGeneration = 0;
    std::uint64_t m_livePreviewRevision = 0;
    std::uint64_t m_pendingLivePreviewGeneration = 0;
    std::uint64_t m_pendingLivePreviewRevision = 0;
    std::uint32_t m_nextStrokeSeed = 1;
    int m_committedStrokeCount = 0;
    bool m_eraserMode = false;
    std::vector<RasterHistoryEntry> m_undoRasterSnapshots;
    std::vector<RasterHistoryEntry> m_redoRasterSnapshots;
};
