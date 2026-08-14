//
// Created by Justmoong on 2026 Jun 04.
//

#pragma once

#include <QColor>
#include <QMetaType>

struct CanvasBrushConfig {
    Q_GADGET
    Q_PROPERTY(QColor color MEMBER color)
    Q_PROPERTY(qreal size MEMBER size)
    Q_PROPERTY(qreal flow MEMBER flow)
    Q_PROPERTY(qreal opacity MEMBER opacity)
    Q_PROPERTY(qreal hardness MEMBER hardness)
    Q_PROPERTY(qreal spacing MEMBER spacing)
    Q_PROPERTY(qreal spacingRatio MEMBER spacingRatio)
    Q_PROPERTY(bool flowEnabled MEMBER flowEnabled)
    Q_PROPERTY(bool opacityEnabled MEMBER opacityEnabled)
    Q_PROPERTY(bool hardnessEnabled MEMBER hardnessEnabled)
    Q_PROPERTY(bool spacingEnabled MEMBER spacingEnabled)
    Q_PROPERTY(qreal pressureCurveMinimum MEMBER pressureCurveMinimum)
    Q_PROPERTY(qreal pressureCurveCenter MEMBER pressureCurveCenter)
    Q_PROPERTY(qreal pressureCurveMaximum MEMBER pressureCurveMaximum)
    Q_PROPERTY(bool pressureToOpacityEnabled MEMBER pressureToOpacityEnabled)

public:
    QColor color = QColor{Qt::black};
    qreal size = 8.0;
    qreal flow = 1.0;
    qreal opacity = 1.0;
    qreal hardness = 1.0;
    qreal spacing = 0.0;
    qreal spacingRatio = 0.0;
    bool flowEnabled = true;
    bool opacityEnabled = true;
    bool hardnessEnabled = true;
    bool spacingEnabled = true;
    qreal pressureCurveMinimum = 0.0;
    qreal pressureCurveCenter = 0.5;
    qreal pressureCurveMaximum = 1.0;
    bool pressureToOpacityEnabled = true;
};

Q_DECLARE_METATYPE(CanvasBrushConfig)
