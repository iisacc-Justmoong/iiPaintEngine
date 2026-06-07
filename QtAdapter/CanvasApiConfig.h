//
// Created by Justmoong on 2026 Jun 07.
//

#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct CanvasViewportConfig {
    Q_GADGET
    Q_PROPERTY(qreal documentX MEMBER documentX)
    Q_PROPERTY(qreal documentY MEMBER documentY)
    Q_PROPERTY(qreal zoom MEMBER zoom)
    Q_PROPERTY(qreal devicePixelRatio MEMBER devicePixelRatio)
    Q_PROPERTY(qreal viewWidth MEMBER viewWidth)
    Q_PROPERTY(qreal viewHeight MEMBER viewHeight)

public:
    qreal documentX = 0.0;
    qreal documentY = 0.0;
    qreal zoom = 1.0;
    qreal devicePixelRatio = 1.0;
    qreal viewWidth = 0.0;
    qreal viewHeight = 0.0;
};

struct CanvasRuntimeConfig {
    Q_GADGET
    Q_PROPERTY(bool livePreviewEnabled MEMBER livePreviewEnabled)
    Q_PROPERTY(int livePreviewFrameIntervalMs MEMBER livePreviewFrameIntervalMs)
    Q_PROPERTY(bool multithreadedEventsEnabled MEMBER multithreadedEventsEnabled)

public:
    bool livePreviewEnabled = true;
    int livePreviewFrameIntervalMs = 8;
    bool multithreadedEventsEnabled = true;
};

struct CanvasStateSnapshot {
    Q_GADGET
    Q_PROPERTY(bool liveStrokeActive MEMBER liveStrokeActive)
    Q_PROPERTY(int strokeCount MEMBER strokeCount)
    Q_PROPERTY(QString inputDevice MEMBER inputDevice)
    Q_PROPERTY(qreal inputPressure MEMBER inputPressure)
    Q_PROPERTY(bool canUndo MEMBER canUndo)
    Q_PROPERTY(bool canRedo MEMBER canRedo)
    Q_PROPERTY(qreal canvasWidth MEMBER canvasWidth)
    Q_PROPERTY(qreal canvasHeight MEMBER canvasHeight)
    Q_PROPERTY(QString toolMode MEMBER toolMode)

public:
    bool liveStrokeActive = false;
    int strokeCount = 0;
    QString inputDevice = QStringLiteral("mouse");
    qreal inputPressure = 1.0;
    bool canUndo = false;
    bool canRedo = false;
    qreal canvasWidth = 0.0;
    qreal canvasHeight = 0.0;
    QString toolMode = QStringLiteral("brush");
};

Q_DECLARE_METATYPE(CanvasViewportConfig)
Q_DECLARE_METATYPE(CanvasRuntimeConfig)
Q_DECLARE_METATYPE(CanvasStateSnapshot)
