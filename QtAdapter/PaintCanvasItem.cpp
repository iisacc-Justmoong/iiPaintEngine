//
// Created by Justmoong on 2026 May 24.
//

#include "PaintCanvasItem.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

#include <algorithm>
#include <cstddef>
#include <cmath>

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

} // namespace

PaintCanvasItem::PaintCanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAntialiasing(false);
    m_rasterizer.spacing = 1.0;
    m_rasterizer.flow = 1.0;
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
    ensureRasterLayerSize();
    clearRasterLayer(m_rasterLayer);
    clearLiveStrokePreview();
    resetInputStrokeBuilder(m_strokeBuilder);
    update();
}

void PaintCanvasItem::setDocumentViewport(qreal documentX, qreal documentY, qreal zoom)
{
    ensureRasterLayerSize();
    clearLiveStrokePreview();
    resetInputStrokeBuilder(m_strokeBuilder);
    m_documentOrigin = {static_cast<Types::Scalar>(documentX), static_cast<Types::Scalar>(documentY)};
    m_zoom = std::max<Types::Scalar>(0.01, static_cast<Types::Scalar>(zoom));
    updateViewportGeometry();
    update();
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
    if (m_rasterLayer.width != nextWidth || m_rasterLayer.height != nextHeight) {
        m_rasterLayer = makeRasterLayer(nextWidth, nextHeight);
    }

    if (m_liveRasterLayer.width != nextWidth || m_liveRasterLayer.height != nextHeight) {
        m_liveRasterLayer = makeRasterLayer(nextWidth, nextHeight);
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
    m_viewport.devicePixelRatio = 1.0;
}

void PaintCanvasItem::handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase)
{
    ensureRasterLayerSize();
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, makeDocumentPointerEvent(event, phase));
    if (result.strokeCompleted) {
        clearLiveStrokePreview();
        commitStroke(result.stroke);
        ++m_nextStrokeSeed;
    } else if (m_strokeBuilder.active) {
        updateLiveStrokePreview();
    }
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
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearRasterLayerRect(m_liveRasterLayer, previousDirtyBounds);

    updateLiveStrokeBuffer(m_liveStrokeBuffer,
                           activeStrokeInput(m_strokeBuilder),
                           currentBrushState(),
                           m_stabilizer);
    m_liveStrokeDeviceDirtyBounds = {};
    if (m_liveStrokeBuffer.active) {
        const BrushState brush = currentBrushState();
        const RasterProjection projection = currentRasterProjection();
        const DirtyRegion dirtyRegion = makeDirtyRegion(deviceBoundsForBrushDabs(m_liveStrokeBuffer.frame.dabs,
                                                                                 brush.rasterizer,
                                                                                 projection));
        m_liveStrokeDeviceDirtyBounds = dirtyRegion.bounds;
        paintRasterSamples(m_liveRasterLayer,
                           projectBrushDabs(m_liveStrokeBuffer.frame.dabs, brush.rasterizer, projection));
    }

    requestTextureUpdate(uniteDevicePixelRects(previousDirtyBounds, m_liveStrokeDeviceDirtyBounds));
}

void PaintCanvasItem::clearLiveStrokePreview()
{
    const DevicePixelRect previousDirtyBounds = m_liveStrokeDeviceDirtyBounds;
    clearLiveStrokeBuffer(m_liveStrokeBuffer);
    clearRasterLayerRect(m_liveRasterLayer, previousDirtyBounds);
    m_liveStrokeDeviceDirtyBounds = {};
    requestTextureUpdate(previousDirtyBounds);
}

void PaintCanvasItem::commitStroke(const StrokeInput &stroke)
{
    const StrokeCommand command = makeStrokeCommand(stroke, currentBrushState(), m_stabilizer);
    const RasterProjection projection = currentRasterProjection();
    const std::vector<RasterSample> samples = projectBrushDabs(command.dabs,
                                                               command.brush.rasterizer,
                                                               projection);
    paintRasterSamples(m_rasterLayer, samples);
    requestTextureUpdate(makeDirtyRegion(deviceBoundsForBrushDabs(command.dabs,
                                                                  command.brush.rasterizer,
                                                                  projection)).bounds);
}

BrushState PaintCanvasItem::currentBrushState() const
{
    return BrushState{m_rasterizer, BrushDynamics{}, StrokeResampler{}, m_nextStrokeSeed};
}
