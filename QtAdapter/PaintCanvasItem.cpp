//
// Created by Justmoong on 2026 May 24.
//

#include "PaintCanvasItem.h"

#include <QImage>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QRect>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <memory>

#include "Layer/RasterLayer.h"
#include "Render/DirtyRegion.h"
#include "Stroke/LiveStroke.h"
#include "Stroke/Rasterizer.h"

namespace {

void drawRasterLayer(QPainter *painter, const RasterLayer &layer)
{
    if (layer.width <= 0 || layer.height <= 0 || layer.pixels.empty()) {
        return;
    }

    const QImage image(reinterpret_cast<const uchar *>(layer.pixels.data()),
                       layer.width,
                       layer.height,
                       static_cast<qsizetype>(layer.width) * 4,
                       QImage::Format_ARGB32);
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

Types::Pixel itemPixelSize(qreal value)
{
    return std::max<Types::Pixel>(0, static_cast<Types::Pixel>(std::ceil(value)));
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

QRect qRectFromDeviceRect(DevicePixelRect rect)
{
    return QRect(rect.origin.x, rect.origin.y, rect.width, rect.height);
}

std::uint32_t argbFromColor(const QColor &color)
{
    const QColor resolved = color.isValid() ? color : QColor{Qt::black};
    return static_cast<std::uint32_t>(resolved.rgba());
}

} // namespace

PaintCanvasItem::PaintCanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
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
    const Types::Scalar nextRatio = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(value));
    if (m_rasterizer.spacingRatio == nextRatio) {
        return;
    }

    m_rasterizer.spacingRatio = nextRatio;
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

void PaintCanvasItem::paint(QPainter *painter)
{
    ensureRasterLayerSize();
    drawRasterLayer(painter, m_rasterLayer);
    if (m_liveStrokeBuffer.active) {
        drawRasterLayer(painter, m_liveRasterLayer);
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

void PaintCanvasItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Press);
    event->accept();
}

void PaintCanvasItem::mouseMoveEvent(QMouseEvent *event)
{
    handleMousePointerEvent(event, PointerEventPhase::Move);
    event->accept();
}

void PaintCanvasItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    handleMousePointerEvent(event, PointerEventPhase::Release);
    event->accept();
}

void PaintCanvasItem::ensureRasterLayerSize()
{
    const Types::Pixel nextWidth = itemPixelSize(width());
    const Types::Pixel nextHeight = itemPixelSize(height());
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
    const Types::Scalar viewWidth = static_cast<Types::Scalar>(std::max<Types::Pixel>(0, m_rasterLayer.width));
    const Types::Scalar viewHeight = static_cast<Types::Scalar>(std::max<Types::Pixel>(0, m_rasterLayer.height));
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

void PaintCanvasItem::handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase)
{
    const bool wasLiveStrokeActive = liveStrokeActive();
    ensureRasterLayerSize();
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, makeDocumentPointerEvent(event, phase));
    if (result.strokeCompleted) {
        clearLiveStrokePreview();
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
    const bool primaryDown = event->buttons().testFlag(Qt::LeftButton)
            || (phase == PointerEventPhase::Press && event->button() == Qt::LeftButton);
    const DocumentPoint documentPosition = documentPointFromViewPoint(
            m_viewport,
            ViewPoint{position.x(), position.y()});

    return PointerEvent{
            PointerDeviceKind::Mouse,
            phase,
            documentPosition,
            1.0,
            static_cast<Types::Scalar>(event->timestamp()),
            pointerButtonFromMouseButton(event->button()),
            primaryDown,
    };
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

    update(qRectFromDeviceRect(clipped));
}

void PaintCanvasItem::updateLiveStrokePreview()
{
    const CanvasLiveStrokeWorkRequest request = currentLiveStrokeWorkRequest();
    const std::uint64_t generation = ++m_livePreviewGeneration;
    if (!m_multithreadedEventsEnabled) {
        applyLiveStrokeWorkResult(generation, runCanvasLiveStrokeWork(request));
        return;
    }

    const QPointer<PaintCanvasItem> self(this);
    m_liveEventThreadPool.start([self, request, generation]() {
        const auto result = std::make_shared<CanvasLiveStrokeWorkResult>(runCanvasLiveStrokeWork(request));
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(),
                                  [self, generation, result]() {
                                      if (!self) {
                                          return;
                                      }
                                      self->applyLiveStrokeWorkResult(generation, *result);
                                  },
                                  Qt::QueuedConnection);
    });
}

void PaintCanvasItem::applyLiveStrokeWorkResult(std::uint64_t generation,
                                                const CanvasLiveStrokeWorkResult &result)
{
    if (generation != m_livePreviewGeneration) {
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
}

void PaintCanvasItem::clearLiveStrokePreview()
{
    ++m_livePreviewGeneration;
    m_liveEventThreadPool.clear();
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
    requestTextureUpdate(result.dirtyBounds);
}

void PaintCanvasItem::emitLiveStrokeActiveChangedIfNeeded(bool previousActive)
{
    if (previousActive != liveStrokeActive()) {
        emit liveStrokeActiveChanged();
    }
}

BrushState PaintCanvasItem::currentBrushState() const
{
    return BrushState{m_rasterizer, BrushDynamics{}, StrokeResampler{}, m_nextStrokeSeed};
}

CanvasLiveStrokeWorkRequest PaintCanvasItem::currentLiveStrokeWorkRequest() const
{
    return CanvasLiveStrokeWorkRequest{
            activeStrokeInput(m_strokeBuilder),
            currentBrushState(),
            m_stabilizer,
            currentRasterProjection(),
    };
}

CanvasCommitStrokeWorkRequest PaintCanvasItem::currentCommitStrokeWorkRequest(const StrokeInput &stroke) const
{
    return CanvasCommitStrokeWorkRequest{
            stroke,
            currentBrushState(),
            m_stabilizer,
            currentRasterProjection(),
    };
}

void PaintCanvasItem::invalidatePendingCanvasEventWork()
{
    ++m_canvasEventRevision;
    ++m_livePreviewGeneration;
    m_liveEventThreadPool.clear();
    m_commitEventThreadPool.clear();
}
