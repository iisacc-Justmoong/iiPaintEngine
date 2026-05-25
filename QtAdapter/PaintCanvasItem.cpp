//
// Created by Justmoong on 2026 May 24.
//

#include "PaintCanvasItem.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

#include "Layer/RasterLayer.h"
#include "Stroke/Rasterizer.h"
#include "Stroke/StrokeCurve.h"

namespace {

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

PointerEvent makePointerEvent(QMouseEvent *event, PointerEventPhase phase)
{
    const QPointF position = event->position();
    const bool primaryDown = event->buttons().testFlag(Qt::LeftButton)
            || (phase == PointerEventPhase::Press && event->button() == Qt::LeftButton);

    return PointerEvent{
            PointerDeviceKind::Mouse,
            phase,
            {position.x(), position.y()},
            1.0,
            static_cast<Types::Scalar>(event->timestamp()),
            pointerButtonFromMouseButton(event->button()),
            primaryDown,
    };
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
    if (m_rasterLayer.width <= 0 || m_rasterLayer.height <= 0 || m_rasterLayer.pixels.empty()) {
        return;
    }

    const QImage image(reinterpret_cast<const uchar *>(m_rasterLayer.pixels.data()),
                       m_rasterLayer.width,
                       m_rasterLayer.height,
                       static_cast<qsizetype>(m_rasterLayer.width) * 4,
                       QImage::Format_ARGB32);
    painter->drawImage(QPointF{0.0, 0.0}, image);
}

void PaintCanvasItem::clear()
{
    ensureRasterLayerSize();
    std::fill(m_rasterLayer.pixels.begin(), m_rasterLayer.pixels.end(), 0x00000000U);
    resetInputStrokeBuilder(m_strokeBuilder);
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
    if (m_rasterLayer.width == nextWidth && m_rasterLayer.height == nextHeight) {
        return;
    }

    m_rasterLayer = makeRasterLayer(nextWidth, nextHeight);
}

void PaintCanvasItem::handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase)
{
    ensureRasterLayerSize();
    const InputStrokeBuildResult result = appendPointerEvent(m_strokeBuilder, makePointerEvent(event, phase));
    if (result.strokeCompleted) {
        commitStroke(result.stroke);
    }
}

void PaintCanvasItem::commitStroke(const StrokeInput &stroke)
{
    const StrokeInput stabilized = stabilizeStrokeInput(stroke, m_stabilizer);
    const StrokeCurve curve = makeStrokeCurve(stabilized);
    const std::vector<RasterSample> samples = rasterizeStrokeCurve(curve, m_rasterizer);
    paintRasterSamples(m_rasterLayer, samples);
    update();
}
