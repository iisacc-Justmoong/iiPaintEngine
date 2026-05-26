//
// Created by Justmoong on 2026 May 24.
//

#include "PaintCanvasItem.h"

#include <QImage>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QPointer>
#include <QRect>
#include <QTabletEvent>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <memory>

#include "Layer/RasterLayer.h"
#include "Input/PressureInput.h"
#include "Render/DirtyRegion.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"

namespace {

Types::Scalar safePixelRatio(Types::Scalar value)
{
    return std::max<Types::Scalar>(0.01, value);
}

Types::Scalar unitSettingOrDefault(Types::Scalar value, Types::Scalar fallback)
{
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0, 1.0);
}

void drawRasterLayer(QPainter *painter, const RasterLayer &layer, Types::Scalar devicePixelRatio)
{
    if (layer.width <= 0 || layer.height <= 0 || layer.pixels.empty()) {
        return;
    }

    QImage image(reinterpret_cast<const uchar *>(layer.pixels.data()),
                 layer.width,
                 layer.height,
                 static_cast<qsizetype>(layer.width) * 4,
                 QImage::Format_ARGB32);
    image.setDevicePixelRatio(safePixelRatio(devicePixelRatio));
    painter->drawImage(QPointF{0.0, 0.0}, image);
}

void clearRasterLayer(RasterLayer &layer)
{
    std::fill(layer.pixels.begin(), layer.pixels.end(), 0x00000000U);
}

void clearRasterLayerRect(RasterLayer &layer, DevicePixelRect dirtyBounds)
{
    const DevicePixelRect layerRect{{0, 0}, layer.width, layer.height};
    const DevicePixelRect clipped = intersectDevicePixelRects(layerRect, dirtyBounds);
    if (isEmpty(clipped)) {
        return;
    }

    for (Types::Pixel y = clipped.origin.y; y < clipped.origin.y + clipped.height; ++y) {
        const std::size_t rowStart = static_cast<std::size_t>(y) * static_cast<std::size_t>(layer.width);
        for (Types::Pixel x = clipped.origin.x; x < clipped.origin.x + clipped.width; ++x) {
            layer.pixels[rowStart + static_cast<std::size_t>(x)] = 0x00000000U;
        }
    }
}

Types::Pixel itemPixelSize(qreal value, Types::Scalar devicePixelRatio)
{
    return std::max<Types::Pixel>(
            0,
            static_cast<Types::Pixel>(std::ceil(static_cast<Types::Scalar>(value)
                                                * safePixelRatio(devicePixelRatio))));
}

PointerButton pointerButtonFromMouseButton(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        return PointerButton::Primary;
    }

    if (button == Qt::RightButton) {
        return PointerButton::Secondary;
    }

    if (button == Qt::MiddleButton) {
        return PointerButton::Middle;
    }

    return PointerButton::None;
}

QRect qRectFromDeviceRect(DevicePixelRect rect, Types::Scalar devicePixelRatio)
{
    const Types::Scalar ratio = safePixelRatio(devicePixelRatio);
    const auto left = static_cast<int>(std::floor(static_cast<Types::Scalar>(rect.origin.x) / ratio));
    const auto top = static_cast<int>(std::floor(static_cast<Types::Scalar>(rect.origin.y) / ratio));
    const auto right = static_cast<int>(std::ceil(static_cast<Types::Scalar>(rect.origin.x + rect.width) / ratio));
    const auto bottom = static_cast<int>(std::ceil(static_cast<Types::Scalar>(rect.origin.y + rect.height) / ratio));
    return QRect(left, top, std::max(0, right - left), std::max(0, bottom - top));
}

std::uint32_t argbFromColor(const QColor &color)
{
    const QColor resolved = color.isValid() ? color : QColor{Qt::black};
    return static_cast<std::uint32_t>(resolved.rgba());
}

bool isTabletEraser(const QPointingDevice *device)
{
    return device != nullptr && device->pointerType() == QPointingDevice::PointerType::Eraser;
}

bool isTabletPointerDevice(const QPointingDevice *device)
{
    if (device == nullptr) {
        return false;
    }

    const QPointingDevice::PointerType pointerType = device->pointerType();
    return pointerType == QPointingDevice::PointerType::Pen
            || pointerType == QPointingDevice::PointerType::Eraser
            || pointerType == QPointingDevice::PointerType::Cursor;
}

Types::Scalar eventPointPressure(const QMouseEvent *event)
{
    if (event->points().isEmpty()) {
        return 1.0;
    }

    return std::clamp(static_cast<Types::Scalar>(event->points().first().pressure()), 0.0, 1.0);
}

Types::Scalar eventPointRotation(const QMouseEvent *event)
{
    if (event->points().isEmpty()) {
        return 0.0;
    }

    return static_cast<Types::Scalar>(event->points().first().rotation());
}

bool hasVariableMousePressure(const QMouseEvent *event)
{
    if (event->points().isEmpty()) {
        return false;
    }

    return pressureInputHasVariablePressure(eventPointPressure(event));
}

bool isPressureAwareMouseEvent(const QMouseEvent *event)
{
    return isTabletPointerDevice(event->pointingDevice())
            || event->source() != Qt::MouseEventNotSynthesized
            || hasVariableMousePressure(event);
}

BrushDynamics pressureSensitiveDynamics()
{
    BrushDynamics dynamics;
    dynamics.pressureToSize = 1.0;
    dynamics.pressureToFlow = 1.0;
    dynamics.pressureToOpacity = 1.0;
    return dynamics;
}

} // namespace

PaintCanvasItem::PaintCanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setAntialiasing(false);
    m_liveEventThreadPool.setMaxThreadCount(1);
    m_liveEventThreadPool.setExpiryTimeout(-1);
    m_commitEventThreadPool.setMaxThreadCount(1);
    m_commitEventThreadPool.setExpiryTimeout(-1);
    m_rasterizer.spacing = 1.0;
    m_rasterizer.flow = 1.0;
}

PaintCanvasItem::~PaintCanvasItem()
{
    m_liveEventThreadPool.waitForDone();
    m_commitEventThreadPool.waitForDone();
}

qreal PaintCanvasItem::documentX() const
{
    return m_documentOrigin.x;
}

void PaintCanvasItem::setDocumentX(qreal value)
{
    if (m_documentOrigin.x == value) {
        return;
    }

    m_documentOrigin.x = static_cast<Types::Scalar>(value);
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
    update();
}

qreal PaintCanvasItem::documentY() const
{
    return m_documentOrigin.y;
}

void PaintCanvasItem::setDocumentY(qreal value)
{
    if (m_documentOrigin.y == value) {
        return;
    }

    m_documentOrigin.y = static_cast<Types::Scalar>(value);
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
    update();
}

qreal PaintCanvasItem::zoom() const
{
    return m_zoom;
}

void PaintCanvasItem::setZoom(qreal value)
{
    const Types::Scalar nextZoom = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(value));
    if (m_zoom == nextZoom) {
        return;
    }

    m_zoom = nextZoom;
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
    update();
}

qreal PaintCanvasItem::canvasDevicePixelRatio() const
{
    return m_devicePixelRatio;
}

void PaintCanvasItem::setCanvasDevicePixelRatio(qreal value)
{
    const Types::Scalar nextRatio = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(value));
    if (m_devicePixelRatio == nextRatio) {
        return;
    }

    m_devicePixelRatio = nextRatio;
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
    update();
}

QColor PaintCanvasItem::brushColor() const
{
    return QColor::fromRgba(m_rasterizer.argb);
}

void PaintCanvasItem::setBrushColor(const QColor &color)
{
    const std::uint32_t nextColor = argbFromColor(color);
    if (m_rasterizer.argb == nextColor) {
        return;
    }

    m_rasterizer.argb = nextColor;
    emit brushChanged();
}

qreal PaintCanvasItem::brushSize() const
{
    if (m_rasterizer.brushSize > 0.0) {
        return m_rasterizer.brushSize;
    }

    return static_cast<qreal>(m_rasterizer.radius) * 2.0;
}

void PaintCanvasItem::setBrushSize(qreal value)
{
    const Types::Scalar nextSize = std::max<Types::Scalar>(1.0, static_cast<Types::Scalar>(value));
    if (m_rasterizer.brushSize == nextSize) {
        return;
    }

    m_rasterizer.brushSize = nextSize;
    m_rasterizer.radius = std::max<Types::Pixel>(1, static_cast<Types::Pixel>(std::lround(nextSize * 0.5)));
    emit brushChanged();
}

qreal PaintCanvasItem::brushSpacing() const
{
    return m_rasterizer.spacing;
}

void PaintCanvasItem::setBrushSpacing(qreal value)
{
    const Types::Scalar nextSpacing = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(value));
    if (m_rasterizer.spacing == nextSpacing) {
        return;
    }

    m_rasterizer.spacing = nextSpacing;
    emit brushChanged();
}

qreal PaintCanvasItem::brushSpacingRatio() const
{
    return m_rasterizer.spacingRatio;
}

void PaintCanvasItem::setBrushSpacingRatio(qreal value)
{
    const Types::Scalar nextRatio = std::clamp(static_cast<Types::Scalar>(value), 0.0, 1.0);
    if (m_rasterizer.spacingRatio == nextRatio) {
        return;
    }

    m_rasterizer.spacingRatio = nextRatio;
    emit brushChanged();
}

bool PaintCanvasItem::brushSpacingEnabled() const
{
    return m_rasterizer.spacingEnabled;
}

void PaintCanvasItem::setBrushSpacingEnabled(bool enabled)
{
    if (m_rasterizer.spacingEnabled == enabled) {
        return;
    }

    m_rasterizer.spacingEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::brushFlow() const
{
    return m_rasterizer.flow;
}

void PaintCanvasItem::setBrushFlow(qreal value)
{
    const Types::Scalar nextFlow = std::clamp(static_cast<Types::Scalar>(value), 0.0, 1.0);
    if (m_rasterizer.flow == nextFlow) {
        return;
    }

    m_rasterizer.flow = nextFlow;
    emit brushChanged();
}

bool PaintCanvasItem::brushFlowEnabled() const
{
    return m_rasterizer.flowEnabled;
}

void PaintCanvasItem::setBrushFlowEnabled(bool enabled)
{
    if (m_rasterizer.flowEnabled == enabled) {
        return;
    }

    m_rasterizer.flowEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::brushOpacity() const
{
    return m_rasterizer.opacity;
}

void PaintCanvasItem::setBrushOpacity(qreal value)
{
    const Types::Scalar nextOpacity = std::clamp(static_cast<Types::Scalar>(value), 0.0, 1.0);
    if (m_rasterizer.opacity == nextOpacity) {
        return;
    }

    m_rasterizer.opacity = nextOpacity;
    emit brushChanged();
}

bool PaintCanvasItem::brushOpacityEnabled() const
{
    return m_rasterizer.opacityEnabled;
}

void PaintCanvasItem::setBrushOpacityEnabled(bool enabled)
{
    if (m_rasterizer.opacityEnabled == enabled) {
        return;
    }

    m_rasterizer.opacityEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::brushHardness() const
{
    return m_rasterizer.hardness;
}

void PaintCanvasItem::setBrushHardness(qreal value)
{
    const Types::Scalar nextHardness = std::clamp(static_cast<Types::Scalar>(value), 0.01, 1.0);
    if (m_rasterizer.hardness == nextHardness) {
        return;
    }

    m_rasterizer.hardness = nextHardness;
    emit brushChanged();
}

bool PaintCanvasItem::brushHardnessEnabled() const
{
    return m_rasterizer.hardnessEnabled;
}

void PaintCanvasItem::setBrushHardnessEnabled(bool enabled)
{
    if (m_rasterizer.hardnessEnabled == enabled) {
        return;
    }

    m_rasterizer.hardnessEnabled = enabled;
    emit brushChanged();
}

qreal PaintCanvasItem::pressureCurveMinimum() const
{
    return m_inputNormalizer.pressureCurveMinimum;
}

void PaintCanvasItem::setPressureCurveMinimum(qreal value)
{
    const Types::Scalar nextMinimum = unitSettingOrDefault(static_cast<Types::Scalar>(value), 0.0);
    const Types::Scalar nextCenter = std::max(m_inputNormalizer.pressureCurveCenter, nextMinimum);
    const Types::Scalar nextMaximum = std::max(m_inputNormalizer.pressureCurveMaximum, nextMinimum);
    if (m_inputNormalizer.pressureCurveMinimum == nextMinimum
            && m_inputNormalizer.pressureCurveCenter == nextCenter
            && m_inputNormalizer.pressureCurveMaximum == nextMaximum) {
        return;
    }

    m_inputNormalizer.pressureCurveMinimum = nextMinimum;
    m_inputNormalizer.pressureCurveCenter = nextCenter;
    m_inputNormalizer.pressureCurveMaximum = nextMaximum;
    emit strokeSettingsChanged();
}

qreal PaintCanvasItem::pressureCurveCenter() const
{
    return m_inputNormalizer.pressureCurveCenter;
}

void PaintCanvasItem::setPressureCurveCenter(qreal value)
{
    const Types::Scalar nextCenter = std::clamp(
            unitSettingOrDefault(static_cast<Types::Scalar>(value), 0.5),
            m_inputNormalizer.pressureCurveMinimum,
            m_inputNormalizer.pressureCurveMaximum);
    if (m_inputNormalizer.pressureCurveCenter == nextCenter) {
        return;
    }

    m_inputNormalizer.pressureCurveCenter = nextCenter;
    emit strokeSettingsChanged();
}

qreal PaintCanvasItem::pressureCurveMaximum() const
{
    return m_inputNormalizer.pressureCurveMaximum;
}

void PaintCanvasItem::setPressureCurveMaximum(qreal value)
{
    const Types::Scalar nextMaximum = unitSettingOrDefault(static_cast<Types::Scalar>(value), 1.0);
    const Types::Scalar nextMinimum = std::min(m_inputNormalizer.pressureCurveMinimum, nextMaximum);
    const Types::Scalar nextCenter = std::clamp(m_inputNormalizer.pressureCurveCenter,
                                                nextMinimum,
                                                nextMaximum);
    if (m_inputNormalizer.pressureCurveMinimum == nextMinimum
            && m_inputNormalizer.pressureCurveCenter == nextCenter
            && m_inputNormalizer.pressureCurveMaximum == nextMaximum) {
        return;
    }

    m_inputNormalizer.pressureCurveMinimum = nextMinimum;
    m_inputNormalizer.pressureCurveCenter = nextCenter;
    m_inputNormalizer.pressureCurveMaximum = nextMaximum;
    emit strokeSettingsChanged();
}

qreal PaintCanvasItem::stabilizerStrength() const
{
    return m_stabilizer.smoothing;
}

void PaintCanvasItem::setStabilizerStrength(qreal value)
{
    const Types::Scalar nextStrength = unitSettingOrDefault(static_cast<Types::Scalar>(value), 0.0);
    if (m_stabilizer.smoothing == nextStrength) {
        return;
    }

    m_stabilizer.smoothing = nextStrength;
    invalidatePendingCanvasEventWork();
    if (m_strokeBuilder.active && m_livePreviewEnabled) {
        updateLiveStrokePreview();
    }
    emit strokeSettingsChanged();
}

bool PaintCanvasItem::livePreviewEnabled() const
{
    return m_livePreviewEnabled;
}

void PaintCanvasItem::setLivePreviewEnabled(bool enabled)
{
    if (m_livePreviewEnabled == enabled) {
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    m_livePreviewEnabled = enabled;
    if (!m_livePreviewEnabled) {
        clearLiveStrokePreview();
    }
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit livePreviewEnabledChanged();
}

bool PaintCanvasItem::multithreadedEventsEnabled() const
{
    return m_multithreadedEventsEnabled;
}

void PaintCanvasItem::setMultithreadedEventsEnabled(bool enabled)
{
    if (m_multithreadedEventsEnabled == enabled) {
        return;
    }

    m_multithreadedEventsEnabled = enabled;
    if (!m_multithreadedEventsEnabled) {
        ++m_livePreviewGeneration;
        ++m_livePreviewRevision;
        m_livePreviewWorkPending = false;
        m_liveEventThreadPool.waitForDone();
        m_commitEventThreadPool.waitForDone();
    }
    emit multithreadedEventsEnabledChanged();
}

bool PaintCanvasItem::liveStrokeActive() const
{
    return m_liveStrokeBuffer.active;
}

int PaintCanvasItem::strokeCount() const
{
    return static_cast<int>(m_nextStrokeSeed - 1);
}

QString PaintCanvasItem::inputDevice() const
{
    switch (m_lastInputDevice) {
        case PointerDeviceKind::Tablet:
            return QStringLiteral("tablet");
        case PointerDeviceKind::Touch:
            return QStringLiteral("touch");
        case PointerDeviceKind::Mouse:
            return QStringLiteral("mouse");
    }

    return QStringLiteral("mouse");
}

qreal PaintCanvasItem::inputPressure() const
{
    return m_lastInputPressure;
}

void PaintCanvasItem::paint(QPainter *painter)
{
    ensureRasterLayerSize();
    drawRasterLayer(painter, m_rasterLayer, m_devicePixelRatio);
    if (m_liveStrokeBuffer.active) {
        drawRasterLayer(painter, m_liveRasterLayer, m_devicePixelRatio);
    }
}

void PaintCanvasItem::clear()
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    invalidatePendingCanvasEventWork();
    ensureRasterLayerSize();
    clearRasterLayer(m_rasterLayer);
    clearLiveStrokePreview();
    resetInputStrokeBuilder(m_strokeBuilder);
    m_nextStrokeSeed = 1;
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit strokeCountChanged();
    update();
}

void PaintCanvasItem::setDocumentViewport(qreal documentX, qreal documentY, qreal zoom)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    invalidatePendingCanvasEventWork();
    ensureRasterLayerSize();
    clearLiveStrokePreview();
    resetInputStrokeBuilder(m_strokeBuilder);
    m_documentOrigin = {static_cast<Types::Scalar>(documentX), static_cast<Types::Scalar>(documentY)};
    m_zoom = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(zoom));
    updateViewportGeometry();
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    emit viewportChanged();
    update();
}

void PaintCanvasItem::resetView()
{
    setDocumentViewport(0.0, 0.0, 1.0);
}

void PaintCanvasItem::panBy(qreal documentDx, qreal documentDy)
{
    setDocumentViewport(m_documentOrigin.x + static_cast<Types::Scalar>(documentDx),
                        m_documentOrigin.y + static_cast<Types::Scalar>(documentDy),
                        m_zoom);
}

void PaintCanvasItem::zoomAt(qreal viewX, qreal viewY, qreal factor)
{
    const Types::Scalar nextZoom = std::max<Types::Scalar>(0.01, m_zoom * static_cast<Types::Scalar>(factor));
    const DocumentPoint anchorBefore = documentPointFromViewPoint(m_viewport, ViewPoint{viewX, viewY});
    const Types::Scalar scale = std::max<Types::Scalar>(0.01, nextZoom);
    m_documentOrigin = {
            anchorBefore.x - static_cast<Types::Scalar>(viewX) / scale,
            anchorBefore.y - static_cast<Types::Scalar>(viewY) / scale,
    };
    m_zoom = nextZoom;
    invalidatePendingCanvasEventWork();
    updateViewportGeometry();
    emit viewportChanged();
    update();
}

void PaintCanvasItem::setBrush(qreal size, const QColor &color, qreal flow, qreal opacity)
{
    setBrushSize(size);
    setBrushColor(color);
    setBrushFlow(flow);
    setBrushOpacity(opacity);
}

bool PaintCanvasItem::event(QEvent *event)
{
    switch (event->type()) {
        case QEvent::TabletPress:
            handleTabletPointerEvent(static_cast<QTabletEvent *>(event), PointerEventPhase::Press);
            event->accept();
            return true;
        case QEvent::TabletMove:
            handleTabletPointerEvent(static_cast<QTabletEvent *>(event), PointerEventPhase::Move);
            event->accept();
            return true;
        case QEvent::TabletRelease:
            handleTabletPointerEvent(static_cast<QTabletEvent *>(event), PointerEventPhase::Release);
            event->accept();
            return true;
        default:
            break;
    }

    return QQuickPaintedItem::event(event);
}

void PaintCanvasItem::mousePressEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePointerEvent(event)) {
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Press);
    event->accept();
}

void PaintCanvasItem::mouseMoveEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePointerEvent(event)) {
        event->accept();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Move);
    event->accept();
}

void PaintCanvasItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (shouldIgnoreMousePointerEvent(event)) {
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Release);
    event->accept();
}

void PaintCanvasItem::ensureRasterLayerSize()
{
    const Types::Pixel nextWidth = itemPixelSize(width(), m_devicePixelRatio);
    const Types::Pixel nextHeight = itemPixelSize(height(), m_devicePixelRatio);
    bool resized = false;
    if (m_rasterLayer.width != nextWidth || m_rasterLayer.height != nextHeight) {
        m_rasterLayer = makeRasterLayer(nextWidth, nextHeight);
        resized = true;
    }

    if (m_liveRasterLayer.width != nextWidth || m_liveRasterLayer.height != nextHeight) {
        m_liveRasterLayer = makeRasterLayer(nextWidth, nextHeight);
        resized = true;
    }

    if (resized) {
        invalidatePendingCanvasEventWork();
    }
    updateViewportGeometry();
}

void PaintCanvasItem::updateViewportGeometry()
{
    const Types::Scalar viewWidth = std::max<Types::Scalar>(0.0, static_cast<Types::Scalar>(width()));
    const Types::Scalar viewHeight = std::max<Types::Scalar>(0.0, static_cast<Types::Scalar>(height()));
    const Types::Scalar zoom = std::max<Types::Scalar>(0.01, m_zoom);
    m_viewport.documentRect = {
            m_documentOrigin,
            viewWidth / zoom,
            viewHeight / zoom,
    };
    m_viewport.viewRect = {{0.0, 0.0}, viewWidth, viewHeight};
    m_viewport.devicePixelRect = {{0, 0}, m_rasterLayer.width, m_rasterLayer.height};
    m_viewport.zoom = zoom;
    m_viewport.devicePixelRatio = std::max<Types::Scalar>(0.01, m_devicePixelRatio);
}

bool PaintCanvasItem::shouldIgnoreMousePointerEvent(QMouseEvent *event)
{
    if (m_tabletPointerActive || m_suppressMouseAfterTablet) {
        if (event->type() == QEvent::MouseButtonRelease && !event->buttons().testFlag(Qt::LeftButton)) {
            m_suppressMouseAfterTablet = false;
        }
        return true;
    }

    return false;
}

void PaintCanvasItem::handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    const PointerEvent pointerEvent = makeDocumentPointerEvent(event, phase);
    noteInputState(pointerEvent);
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, pointerEvent);
    if (result.strokeCompleted) {
        preserveLiveStrokePreviewForCommit();
        commitStroke(result.stroke);
        ++m_nextStrokeSeed;
        emit strokeCountChanged();
    } else if (m_strokeBuilder.active && m_livePreviewEnabled) {
        updateLiveStrokePreview();
    }
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
}

void PaintCanvasItem::handleTabletPointerEvent(QTabletEvent *event, PointerEventPhase phase)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    const PointerEvent pointerEvent = makeDocumentPointerEvent(event, phase);
    noteTabletPointerEvent(pointerEvent);
    noteInputState(pointerEvent);
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, pointerEvent);
    if (result.strokeCompleted) {
        preserveLiveStrokePreviewForCommit();
        commitStroke(result.stroke);
        ++m_nextStrokeSeed;
        emit strokeCountChanged();
    } else if (m_strokeBuilder.active && m_livePreviewEnabled) {
        updateLiveStrokePreview();
    }
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
}

PointerEvent PaintCanvasItem::makeDocumentPointerEvent(QMouseEvent *event, PointerEventPhase phase) const
{
    const QPointF position = event->position();
    const bool pressureAware = isPressureAwareMouseEvent(event);
    const bool eraserActive = pressureAware && isTabletEraser(event->pointingDevice());
    const Types::Scalar pressure = pressureAware
            ? resolvePressureInput(PressureInput{
                    true,
                    0.0,
                    1.0,
                    eventPointPressure(event),
                    true,
                    false,
                    m_inputNormalizer.pressureCurveMinimum,
                    m_inputNormalizer.pressureCurveCenter,
                    m_inputNormalizer.pressureCurveMaximum,
            })
            : 1.0;
    const bool pressureContact = pressureAware && pressureInputHasContact(pressure);
    const bool primaryDown = event->buttons().testFlag(Qt::LeftButton)
            || (phase == PointerEventPhase::Press && event->button() == Qt::LeftButton)
            || pressureContact
            || eraserActive;
    const DocumentPoint documentPosition = documentPointFromViewPoint(
            m_viewport,
            ViewPoint{position.x(), position.y()});

    PointerEvent pointerEvent;
    pointerEvent.device = pressureAware ? PointerDeviceKind::Tablet : PointerDeviceKind::Mouse;
    pointerEvent.phase = phase;
    pointerEvent.documentPosition = documentPosition;
    pointerEvent.pressure = pressure;
    pointerEvent.time = static_cast<Types::Scalar>(event->timestamp());
    pointerEvent.button = eraserActive
            ? PointerButton::Eraser
            : (pressureContact ? PointerButton::Primary : pointerButtonFromMouseButton(event->button()));
    pointerEvent.primaryButtonDown = primaryDown;
    pointerEvent.tool = eraserActive
            ? PointerToolKind::Eraser
            : (pressureAware ? PointerToolKind::Pen : PointerToolKind::Mouse);
    pointerEvent.eraserActive = eraserActive;
    pointerEvent.rotationRadians = pressureAware ? eventPointRotation(event) : 0.0;
    if (primaryDown) {
        pointerEvent.deviceState |= PointerDeviceStatePrimaryButton;
    }
    if (eraserActive) {
        pointerEvent.deviceState |= PointerDeviceStateEraser;
    }
    return pointerEvent;
}

PointerEvent PaintCanvasItem::makeDocumentPointerEvent(QTabletEvent *event, PointerEventPhase phase) const
{
    return normalizeTabletPointerEvent(m_inputNormalizer, makeTabletState(event, phase), phase);
}

void PaintCanvasItem::noteTabletPointerEvent(const PointerEvent &event)
{
    const bool contact = event.primaryButtonDown || event.pressure > 0.0 || event.eraserActive;
    if ((event.phase == PointerEventPhase::Press || event.phase == PointerEventPhase::Move) && contact) {
        m_tabletPointerActive = true;
        m_suppressMouseAfterTablet = true;
        return;
    }

    if (event.phase == PointerEventPhase::Release || event.phase == PointerEventPhase::Cancel) {
        const bool suppressMouseFallback = m_tabletPointerActive || contact;
        m_tabletPointerActive = false;
        if (suppressMouseFallback) {
            m_suppressMouseAfterTablet = true;
        }
    }
}

TabletState PaintCanvasItem::makeTabletState(QTabletEvent *event, PointerEventPhase phase) const
{
    const QPointF position = event->position();
    const bool eraserActive = isTabletEraser(event->pointingDevice());
    const Types::Scalar pressure = std::clamp(static_cast<Types::Scalar>(event->pressure()), 0.0, 1.0);
    const bool pressureContact = pressure > 0.0;
    const bool primaryDown = event->buttons().testFlag(Qt::LeftButton)
            || (phase == PointerEventPhase::Press && event->button() == Qt::LeftButton)
            || pressureContact
            || eraserActive;
    const DocumentPoint documentPosition = documentPointFromViewPoint(
            m_viewport,
            ViewPoint{position.x(), position.y()});

    TabletState tablet;
    tablet.documentPosition = documentPosition;
    tablet.pressure = pressure;
    tablet.tiltX = std::clamp(static_cast<Types::Scalar>(event->xTilt()) / 60.0, -1.0, 1.0);
    tablet.tiltY = std::clamp(static_cast<Types::Scalar>(event->yTilt()) / 60.0, -1.0, 1.0);
    tablet.rotationRadians = static_cast<Types::Scalar>(event->rotation()) * 3.14159265358979323846 / 180.0;
    tablet.time = static_cast<Types::Scalar>(event->timestamp());
    tablet.inProximity = true;
    tablet.contact = primaryDown;
    tablet.primaryButtonDown = primaryDown;
    tablet.hovering = phase == PointerEventPhase::Move && !primaryDown && pressure <= 0.0;
    tablet.barrelButtonDown = event->buttons().testFlag(Qt::RightButton) || event->button() == Qt::RightButton;
    tablet.eraser = eraserActive;
    tablet.tool = eraserActive ? TabletToolKind::Eraser : TabletToolKind::Pen;
    return tablet;
}

RasterProjection PaintCanvasItem::currentRasterProjection() const
{
    return RasterProjection{
            m_viewport.documentRect.origin,
            m_viewport.devicePixelRect.origin,
            m_viewport.zoom * m_viewport.devicePixelRatio,
    };
}

DevicePixelRect PaintCanvasItem::layerBounds() const
{
    return DevicePixelRect{{0, 0}, m_rasterLayer.width, m_rasterLayer.height};
}

void PaintCanvasItem::requestTextureUpdate(DevicePixelRect dirtyBounds)
{
    const DevicePixelRect clipped = intersectDevicePixelRects(layerBounds(), dirtyBounds);
    if (isEmpty(clipped)) {
        return;
    }

    update(qRectFromDeviceRect(clipped, m_devicePixelRatio));
}

void PaintCanvasItem::updateLiveStrokePreview()
{
    const CanvasLiveStrokeWorkRequest request = currentLiveStrokeWorkRequest();
    const std::uint64_t generation = ++m_livePreviewGeneration;
    const std::uint64_t revision = m_livePreviewRevision;
    if (!m_multithreadedEventsEnabled) {
        applyLiveStrokeWorkResult(generation, revision, runCanvasLiveStrokeWork(request));
        return;
    }

    if (m_livePreviewWorkActive) {
        m_pendingLiveStrokeWorkRequest = request;
        m_pendingLivePreviewGeneration = generation;
        m_pendingLivePreviewRevision = revision;
        m_livePreviewWorkPending = true;
        return;
    }

    startLiveStrokePreviewWork(request, generation, revision);
}

void PaintCanvasItem::startLiveStrokePreviewWork(const CanvasLiveStrokeWorkRequest &request,
                                                 std::uint64_t generation,
                                                 std::uint64_t revision)
{
    m_livePreviewWorkActive = true;
    const QPointer<PaintCanvasItem> self(this);
    m_liveEventThreadPool.start([self, request, generation, revision]() {
        const auto result = std::make_shared<CanvasLiveStrokeWorkResult>(runCanvasLiveStrokeWork(request));
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(),
                                  [self, generation, revision, result]() {
                                      if (!self) {
                                          return;
                                      }
                                      self->applyLiveStrokeWorkResult(generation, revision, *result);
                                  },
                                  Qt::QueuedConnection);
    });
}

void PaintCanvasItem::startPendingLiveStrokePreviewWork()
{
    if (m_livePreviewWorkActive || !m_livePreviewWorkPending) {
        return;
    }

    if (!m_multithreadedEventsEnabled) {
        m_livePreviewWorkPending = false;
        return;
    }

    const CanvasLiveStrokeWorkRequest request = m_pendingLiveStrokeWorkRequest;
    const std::uint64_t generation = m_pendingLivePreviewGeneration;
    const std::uint64_t revision = m_pendingLivePreviewRevision;
    m_livePreviewWorkPending = false;

    if (revision != m_livePreviewRevision || !m_livePreviewEnabled || !m_strokeBuilder.active) {
        return;
    }

    startLiveStrokePreviewWork(request, generation, revision);
}

void PaintCanvasItem::applyLiveStrokeWorkResult(std::uint64_t generation,
                                                std::uint64_t revision,
                                                const CanvasLiveStrokeWorkResult &result)
{
    m_livePreviewWorkActive = false;
    if (revision != m_livePreviewRevision
            || generation > m_livePreviewGeneration
            || !m_livePreviewEnabled
            || !m_strokeBuilder.active) {
        startPendingLiveStrokePreviewWork();
        return;
    }

    const bool wasLiveStrokeActive = liveStrokeActive();
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearRasterLayerRect(m_liveRasterLayer, previousDirtyBounds);

    m_liveStrokeBuffer.frame = result.frame;
    m_liveStrokeBuffer.active = result.frame.active;
    m_liveStrokeDeviceDirtyBounds = result.frame.active ? result.dirtyBounds : DevicePixelRect{};
    if (m_liveStrokeBuffer.active) {
        paintRasterSamples(m_liveRasterLayer, result.samples);
    }

    requestTextureUpdate(uniteDevicePixelRects(previousDirtyBounds, m_liveStrokeDeviceDirtyBounds));
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
    startPendingLiveStrokePreviewWork();
}

void PaintCanvasItem::preserveLiveStrokePreviewForCommit()
{
    ++m_livePreviewGeneration;
    ++m_livePreviewRevision;
    m_livePreviewWorkPending = false;
}

void PaintCanvasItem::clearLiveStrokePreview()
{
    ++m_livePreviewGeneration;
    ++m_livePreviewRevision;
    m_livePreviewWorkPending = false;
    clearLiveStrokePreviewPixels();
}

void PaintCanvasItem::clearLiveStrokePreviewPixels()
{
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearLiveStrokeBuffer(m_liveStrokeBuffer);
    clearRasterLayerRect(m_liveRasterLayer, previousDirtyBounds);
    m_liveStrokeDeviceDirtyBounds = {};
    requestTextureUpdate(previousDirtyBounds);
}

void PaintCanvasItem::commitStroke(const StrokeInput &stroke)
{
    const CanvasCommitStrokeWorkRequest request = currentCommitStrokeWorkRequest(stroke);
    const std::uint64_t revision = m_canvasEventRevision;
    if (!m_multithreadedEventsEnabled) {
        applyCommitStrokeWorkResult(revision, runCanvasCommitStrokeWork(request));
        return;
    }

    const QPointer<PaintCanvasItem> self(this);
    m_commitEventThreadPool.start([self, request, revision]() {
        const auto result = std::make_shared<CanvasCommitStrokeWorkResult>(runCanvasCommitStrokeWork(request));
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(),
                                  [self, revision, result]() {
                                      if (!self) {
                                          return;
                                      }
                                      self->applyCommitStrokeWorkResult(revision, *result);
                                  },
                                  Qt::QueuedConnection);
    });
}

void PaintCanvasItem::applyCommitStrokeWorkResult(std::uint64_t revision,
                                                  const CanvasCommitStrokeWorkResult &result)
{
    if (revision != m_canvasEventRevision) {
        return;
    }

    paintRasterSamples(m_rasterLayer, result.samples);
    const bool wasLiveStrokeActive = liveStrokeActive();
    const DevicePixelRect previousLiveDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearLiveStrokePreviewPixels();
    requestTextureUpdate(uniteDevicePixelRects(result.dirtyBounds, previousLiveDirtyBounds));
    emitLiveStrokeActiveChangedIfNeeded(wasLiveStrokeActive);
}

void PaintCanvasItem::emitLiveStrokeActiveChangedIfNeeded(bool previousActive)
{
    if (previousActive != liveStrokeActive()) {
        emit liveStrokeActiveChanged();
    }
}

void PaintCanvasItem::noteInputState(const PointerEvent &event)
{
    const Types::Scalar pressure = std::clamp(event.pressure, 0.0, 1.0);
    if (m_lastInputDevice == event.device
            && std::abs(m_lastInputPressure - pressure) < 0.001) {
        return;
    }

    m_lastInputDevice = event.device;
    m_lastInputPressure = pressure;
    emit inputStateChanged();
}

BrushState PaintCanvasItem::currentBrushState() const
{
    return BrushState{m_rasterizer, pressureSensitiveDynamics(), StrokeResampler{}, BrushMaterial{}, m_nextStrokeSeed};
}

CanvasLiveStrokeWorkRequest PaintCanvasItem::currentLiveStrokeWorkRequest() const
{
    return CanvasLiveStrokeWorkRequest{
            activeStrokeInput(m_strokeBuilder),
            currentBrushState(),
            m_stabilizer,
            currentRasterProjection(),
            m_rasterLayer,
    };
}

CanvasCommitStrokeWorkRequest PaintCanvasItem::currentCommitStrokeWorkRequest(const StrokeInput &stroke) const
{
    return CanvasCommitStrokeWorkRequest{
            stroke,
            currentBrushState(),
            m_stabilizer,
            currentRasterProjection(),
            m_rasterLayer,
    };
}

void PaintCanvasItem::invalidatePendingCanvasEventWork()
{
    ++m_canvasEventRevision;
    ++m_livePreviewGeneration;
    ++m_livePreviewRevision;
    m_livePreviewWorkPending = false;
    m_commitEventThreadPool.clear();
}
