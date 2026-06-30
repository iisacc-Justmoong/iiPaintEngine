//
// Created by Justmoong on 2026 Jun 04.
//

#include "CanvasAdapter.h"

#include <QImage>
#include <QQuickItem>
#include <QUrl>

#include <algorithm>

namespace {

QString localFilePath(const QString &filePath)
{
    const QUrl url(filePath);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    return filePath;
}

QString normalizedToolMode(const QString &mode)
{
    const QString trimmed = mode.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("brush");
    }
    return trimmed;
}

} // namespace

CanvasAdapter::CanvasAdapter(QQuickItem *parent)
    : PaintCanvasItem(parent)
{
    connect(this, &PaintCanvasItem::brushChanged, this, &CanvasAdapter::brushConfigChanged);
    connect(this, &PaintCanvasItem::strokeSettingsChanged, this, &CanvasAdapter::brushConfigChanged);
    connect(this, &PaintCanvasItem::viewportChanged, this, &CanvasAdapter::viewportConfigChanged);
    connect(this, &PaintCanvasItem::livePreviewEnabledChanged, this, &CanvasAdapter::runtimeConfigChanged);
    connect(this, &PaintCanvasItem::livePreviewFrameIntervalMsChanged, this, &CanvasAdapter::runtimeConfigChanged);
    connect(this, &PaintCanvasItem::multithreadedEventsEnabledChanged, this, &CanvasAdapter::runtimeConfigChanged);
    connect(this, &PaintCanvasItem::liveStrokeActiveChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &PaintCanvasItem::strokeCountChanged, this, &CanvasAdapter::undoRedoChanged);
    connect(this, &PaintCanvasItem::strokeCountChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &PaintCanvasItem::inputStateChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &PaintCanvasItem::viewportChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &QQuickItem::widthChanged, this, &CanvasAdapter::viewportConfigChanged);
    connect(this, &QQuickItem::heightChanged, this, &CanvasAdapter::viewportConfigChanged);
    connect(this, &QQuickItem::widthChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &QQuickItem::heightChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &CanvasAdapter::toolModeChanged, this, &CanvasAdapter::stateSnapshotChanged);
    connect(this, &CanvasAdapter::undoRedoChanged, this, &CanvasAdapter::stateSnapshotChanged);
}

QString CanvasAdapter::toolMode() const
{
    return m_toolMode;
}

void CanvasAdapter::setToolMode(const QString &mode)
{
    const QString nextMode = normalizedToolMode(mode);
    if (m_toolMode == nextMode) {
        return;
    }

    m_toolMode = nextMode;
    setEraserMode(m_toolMode == QStringLiteral("eraser"));
    emit toolModeChanged();
}

CanvasBrushConfig CanvasAdapter::brushConfig() const
{
    CanvasBrushConfig config;
    config.color = brushColor();
    config.size = brushSize();
    config.flow = brushFlow();
    config.opacity = brushOpacity();
    config.hardness = brushHardness();
    config.spacing = brushSpacing();
    config.spacingRatio = brushSpacingRatio();
    config.flowEnabled = brushFlowEnabled();
    config.opacityEnabled = brushOpacityEnabled();
    config.hardnessEnabled = brushHardnessEnabled();
    config.spacingEnabled = brushSpacingEnabled();
    config.pressureCurveMinimum = pressureCurveMinimum();
    config.pressureCurveCenter = pressureCurveCenter();
    config.pressureCurveMaximum = pressureCurveMaximum();
    config.stabilizerStrength = stabilizerStrength();
    return config;
}

void CanvasAdapter::setBrushConfig(const CanvasBrushConfig &config)
{
    setBrushSize(config.size);
    setBrushColor(config.color);
    setBrushFlow(config.flow);
    setBrushOpacity(config.opacity);
    setBrushHardness(config.hardness);
    setBrushSpacing(config.spacing);
    setBrushSpacingRatio(config.spacingRatio);
    setBrushFlowEnabled(config.flowEnabled);
    setBrushOpacityEnabled(config.opacityEnabled);
    setBrushHardnessEnabled(config.hardnessEnabled);
    setBrushSpacingEnabled(config.spacingEnabled);
    setPressureCurveMinimum(config.pressureCurveMinimum);
    setPressureCurveMaximum(config.pressureCurveMaximum);
    setPressureCurveCenter(config.pressureCurveCenter);
    setStabilizerStrength(config.stabilizerStrength);
}

CanvasViewportConfig CanvasAdapter::viewportConfig() const
{
    CanvasViewportConfig config;
    config.documentX = documentX();
    config.documentY = documentY();
    config.zoom = zoom();
    config.devicePixelRatio = canvasDevicePixelRatio();
    config.viewWidth = width();
    config.viewHeight = height();
    return config;
}

void CanvasAdapter::setViewportConfig(const CanvasViewportConfig &config)
{
    setWidth(std::max<qreal>(0.0, config.viewWidth));
    setHeight(std::max<qreal>(0.0, config.viewHeight));
    setCanvasDevicePixelRatio(config.devicePixelRatio);
    setDocumentViewport(config.documentX, config.documentY, config.zoom);
}

CanvasRuntimeConfig CanvasAdapter::runtimeConfig() const
{
    CanvasRuntimeConfig config;
    config.livePreviewEnabled = livePreviewEnabled();
    config.livePreviewFrameIntervalMs = livePreviewFrameIntervalMs();
    config.multithreadedEventsEnabled = multithreadedEventsEnabled();
    return config;
}

void CanvasAdapter::setRuntimeConfig(const CanvasRuntimeConfig &config)
{
    setLivePreviewEnabled(config.livePreviewEnabled);
    setLivePreviewFrameIntervalMs(config.livePreviewFrameIntervalMs);
    setMultithreadedEventsEnabled(config.multithreadedEventsEnabled);
}

CanvasStateSnapshot CanvasAdapter::stateSnapshot() const
{
    CanvasStateSnapshot snapshot;
    snapshot.liveStrokeActive = liveStrokeActive();
    snapshot.strokeCount = strokeCount();
    snapshot.inputDevice = inputDevice();
    snapshot.inputPressure = inputPressure();
    snapshot.canUndo = canUndo();
    snapshot.canRedo = canRedo();
    snapshot.canvasWidth = width();
    snapshot.canvasHeight = height();
    snapshot.toolMode = toolMode();
    return snapshot;
}

bool CanvasAdapter::canUndo() const
{
    return canUndoRasterChange();
}

bool CanvasAdapter::canRedo() const
{
    return canRedoRasterChange();
}

bool CanvasAdapter::newCanvas(int width, int height)
{
    const bool created = resetRasterCanvas(width, height);
    if (created) {
        emit undoRedoChanged();
    }
    return created;
}

bool CanvasAdapter::openRaster(const QString &filePath)
{
    const QImage image(localFilePath(filePath));
    const bool opened = replaceRasterCanvas(image);
    if (opened) {
        emit undoRedoChanged();
    }
    return opened;
}

bool CanvasAdapter::saveToFile(const QString &filePath)
{
    return saveRasterCanvasToFile(localFilePath(filePath));
}

bool CanvasAdapter::undo()
{
    const bool applied = undoRasterChange();
    if (applied) {
        emit undoRedoChanged();
    }
    return applied;
}

bool CanvasAdapter::redo()
{
    const bool applied = redoRasterChange();
    if (applied) {
        emit undoRedoChanged();
    }
    return applied;
}
