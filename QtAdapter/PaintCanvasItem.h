//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <QQuickPaintedItem>

#include "Input/InputStrokeBuilder.h"
#include "Layer/RasterLayer.h"
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

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void ensureRasterLayerSize();
    void handleMousePointerEvent(QMouseEvent *event, PointerEventPhase phase);
    void commitStroke(const StrokeInput &stroke);

    RasterLayer m_rasterLayer;
    InputStrokeBuilder m_strokeBuilder;
    Stabilizer m_stabilizer{0.25};
    Rasterizer m_rasterizer{};
};
