#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

#include "BitmapFile/BitmapFile.h"
#include "Input/InputNormalizer.h"
#include "Input/InputStrokeBuilder.h"
#include "QtAdapter/BitmapBrushConfig.h"
#include "QtAdapter/BitmapFileApiConfig.h"
#include "Render/DirtyRegion.h"
#include "Stroke/Rasterizer.h"
#include "Transform/ViewportTransform.h"

class QEvent;
class QMouseEvent;
class QPainter;
class QTabletEvent;

class BitmapFileItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(QString fileFormat READ fileFormat NOTIFY fileChanged)
    Q_PROPERTY(bool fileOpen READ fileOpen NOTIFY fileChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY fileChanged)
    Q_PROPERTY(bool pixelWritable READ pixelWritable NOTIFY fileChanged)
    Q_PROPERTY(bool canSaveInPlace READ canSaveInPlace NOTIFY fileChanged)
    Q_PROPERTY(int bitmapWidth READ bitmapWidth NOTIFY fileChanged)
    Q_PROPERTY(int bitmapHeight READ bitmapHeight NOTIFY fileChanged)
    Q_PROPERTY(QStringList supportedOpenFormats READ supportedOpenFormats CONSTANT)
    Q_PROPERTY(QStringList supportedSaveFormats READ supportedSaveFormats CONSTANT)
    Q_PROPERTY(QStringList supportedEditableFormats READ supportedEditableFormats CONSTANT)
    Q_PROPERTY(QString lastFileError READ lastFileError NOTIFY lastFileErrorChanged)
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(BitmapBrushConfig brushConfig READ brushConfig WRITE setBrushConfig NOTIFY brushConfigChanged)
    Q_PROPERTY(BitmapViewportConfig viewportConfig READ viewportConfig WRITE setViewportConfig NOTIFY viewportConfigChanged)
    Q_PROPERTY(BitmapRuntimeConfig runtimeConfig READ runtimeConfig WRITE setRuntimeConfig NOTIFY runtimeConfigChanged)
    Q_PROPERTY(BitmapFileState stateSnapshot READ stateSnapshot NOTIFY stateSnapshotChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)
    Q_PROPERTY(qreal documentX READ documentX WRITE setDocumentX NOTIFY viewportChanged)
    Q_PROPERTY(qreal documentY READ documentY WRITE setDocumentY NOTIFY viewportChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewportChanged)
    Q_PROPERTY(qreal bitmapDevicePixelRatio READ bitmapDevicePixelRatio WRITE setBitmapDevicePixelRatio NOTIFY viewportChanged)
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
    Q_PROPERTY(bool pressureToOpacityEnabled READ pressureToOpacityEnabled WRITE setPressureToOpacityEnabled NOTIFY brushChanged)
    Q_PROPERTY(bool livePreviewEnabled READ livePreviewEnabled WRITE setLivePreviewEnabled NOTIFY livePreviewEnabledChanged)
    Q_PROPERTY(bool liveStrokeActive READ liveStrokeActive NOTIFY liveStrokeActiveChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokeCountChanged)
    Q_PROPERTY(QString inputDevice READ inputDevice NOTIFY inputStateChanged)
    Q_PROPERTY(qreal inputPressure READ inputPressure NOTIFY inputStateChanged)

public:
    explicit BitmapFileItem(QQuickItem *parent = nullptr);
    ~BitmapFileItem() override;

    void paint(QPainter *painter) override;

    QString filePath() const;
    QString fileFormat() const;
    bool fileOpen() const;
    bool modified() const;
    bool pixelWritable() const;
    bool canSaveInPlace() const;
    int bitmapWidth() const;
    int bitmapHeight() const;
    QStringList supportedOpenFormats() const;
    QStringList supportedSaveFormats() const;
    QStringList supportedEditableFormats() const;
    QString lastFileError() const;

    QString toolMode() const;
    void setToolMode(const QString &mode);
    BitmapBrushConfig brushConfig() const;
    Q_INVOKABLE void setBrushConfig(const BitmapBrushConfig &config);
    BitmapViewportConfig viewportConfig() const;
    Q_INVOKABLE void setViewportConfig(const BitmapViewportConfig &config);
    BitmapRuntimeConfig runtimeConfig() const;
    Q_INVOKABLE void setRuntimeConfig(const BitmapRuntimeConfig &config);
    BitmapFileState stateSnapshot() const;
    bool canUndo() const;
    bool canRedo() const;

    Q_INVOKABLE bool createFile(const QString &filePath, int width, int height);
    Q_INVOKABLE bool createFile(const QString &filePath,
                                int width,
                                int height,
                                const QString &format);
    Q_INVOKABLE bool openFile(const QString &filePath);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool save(int quality);
    Q_INVOKABLE bool saveAs(const QString &filePath);
    Q_INVOKABLE bool saveAs(const QString &filePath, const QString &format);
    Q_INVOKABLE bool saveAs(const QString &filePath,
                            const QString &format,
                            int quality);
    Q_INVOKABLE bool clear();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    QImage bitmapImage() const;

    qreal documentX() const;
    void setDocumentX(qreal value);
    qreal documentY() const;
    void setDocumentY(qreal value);
    qreal zoom() const;
    void setZoom(qreal value);
    qreal bitmapDevicePixelRatio() const;
    void setBitmapDevicePixelRatio(qreal value);
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
    bool pressureToOpacityEnabled() const;
    void setPressureToOpacityEnabled(bool enabled);
    bool livePreviewEnabled() const;
    void setLivePreviewEnabled(bool enabled);
    bool liveStrokeActive() const;
    int strokeCount() const;
    QString inputDevice() const;
    qreal inputPressure() const;

    Q_INVOKABLE void setDocumentViewport(qreal documentX, qreal documentY, qreal zoom);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void panBy(qreal documentDx, qreal documentDy);
    Q_INVOKABLE void zoomAt(qreal viewX, qreal viewY, qreal factor);
    Q_INVOKABLE void setBrush(qreal size, const QColor &color, qreal flow, qreal opacity);

signals:
    void fileChanged();
    void lastFileErrorChanged();
    void toolModeChanged();
    void brushConfigChanged();
    void viewportConfigChanged();
    void runtimeConfigChanged();
    void stateSnapshotChanged();
    void undoRedoChanged();
    void viewportChanged();
    void brushChanged();
    void strokeSettingsChanged();
    void livePreviewEnabledChanged();
    void liveStrokeActiveChanged();
    void strokeCountChanged();
    void inputStateChanged();

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

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
        bool fullBitmap = true;
        RasterSnapshot fullSnapshot;
        RasterPatchSnapshot patchSnapshot;
    };

    void resetInteractionForFile();
    void ensureBitmapBuffers();
    void updateViewportGeometry();
    bool shouldIgnoreMousePointerEvent(QMouseEvent *event);
    void handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase);
    PointerEvent makeDocumentPointerEvent(QMouseEvent *event, PointerEventPhase phase) const;
    void handleTabletPointerEvent(QTabletEvent *event, PointerEventPhase phase);
    void noteTabletPointerEvent(const PointerEvent &event);
    TabletState makeTabletState(QTabletEvent *event, PointerEventPhase phase) const;
    PointerEvent makeDocumentPointerEvent(QTabletEvent *event, PointerEventPhase phase) const;
    RasterProjection currentRasterProjection() const;
    DevicePixelRect bitmapBounds() const;
    void requestTextureUpdate(DevicePixelRect dirtyBounds);
    void applyPointerBuildResult(const InputStrokeBuildResult &result);
    void appendPointerPointToRaster(const StrokePoint &point, bool finishStroke);
    void commitPendingRasterStroke();
    void clearPendingRasterStroke();
    void syncPendingRasterSamples(const std::vector<RasterSample> &samples);
    void emitLiveStrokeActiveChangedIfNeeded(bool previousActive);
    void noteInputState(const PointerEvent &event);
    BrushState currentBrushState() const;
    void cancelActiveRasterStroke();
    RasterSnapshot captureRasterSnapshot() const;
    RasterPatchSnapshot captureRasterPatchSnapshot(DevicePixelRect dirtyBounds) const;
    RasterHistoryEntry captureRasterHistoryEntry() const;
    RasterHistoryEntry captureRasterHistoryEntry(DevicePixelRect dirtyBounds) const;
    void recordRasterChange();
    void recordRasterChange(DevicePixelRect dirtyBounds);
    void restoreRasterSnapshot(const RasterSnapshot &snapshot);
    void restoreRasterPatchSnapshot(const RasterPatchSnapshot &snapshot);
    void restoreRasterHistoryEntry(const RasterHistoryEntry &entry);
    void emitFileStateSignals();

    BitmapFile m_bitmapFile;
    RasterLayer m_liveRasterLayer;
    StrokeCompositeBuffer m_pendingRasterBuffer;
    InputNormalizer m_inputNormalizer;
    InputStrokeBuilder m_strokeBuilder;
    RasterDabStream m_rasterDabStream;
    BrushState m_activeBrush;
    Rasterizer m_rasterizer{};
    RasterViewport m_viewport{};
    DocumentPoint m_documentOrigin{};
    Types::Scalar m_zoom = 1.0;
    Types::Scalar m_devicePixelRatio = 1.0;
    DevicePixelRect m_liveStrokeDeviceDirtyBounds{};
    bool m_livePreviewEnabled = true;
    bool m_liveStrokePreviewDestinationOut = false;
    bool m_tabletPointerActive = false;
    bool m_suppressMouseAfterTablet = false;
    PointerDeviceKind m_lastInputDevice = PointerDeviceKind::Mouse;
    Types::Scalar m_lastInputPressure = 1.0;
    std::uint32_t m_nextStrokeSeed = 1;
    int m_committedStrokeCount = 0;
    bool m_eraserMode = false;
    bool m_pressureToOpacityEnabled = true;
    QString m_toolMode = QStringLiteral("brush");
    std::vector<RasterHistoryEntry> m_undoRasterSnapshots;
    std::vector<RasterHistoryEntry> m_redoRasterSnapshots;
};
