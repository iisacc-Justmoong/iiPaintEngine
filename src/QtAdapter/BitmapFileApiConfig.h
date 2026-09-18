#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct BitmapViewportConfig {
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

struct BitmapRuntimeConfig {
    Q_GADGET
    Q_PROPERTY(bool livePreviewEnabled MEMBER livePreviewEnabled)

public:
    bool livePreviewEnabled = true;
};

struct BitmapFileState {
    Q_GADGET
    Q_PROPERTY(bool open MEMBER open)
    Q_PROPERTY(bool modified MEMBER modified)
    Q_PROPERTY(bool pixelWritable MEMBER pixelWritable)
    Q_PROPERTY(bool canSaveInPlace MEMBER canSaveInPlace)
    Q_PROPERTY(bool liveStrokeActive MEMBER liveStrokeActive)
    Q_PROPERTY(int strokeCount MEMBER strokeCount)
    Q_PROPERTY(QString inputDevice MEMBER inputDevice)
    Q_PROPERTY(qreal inputPressure MEMBER inputPressure)
    Q_PROPERTY(bool canUndo MEMBER canUndo)
    Q_PROPERTY(bool canRedo MEMBER canRedo)
    Q_PROPERTY(int bitmapWidth MEMBER bitmapWidth)
    Q_PROPERTY(int bitmapHeight MEMBER bitmapHeight)
    Q_PROPERTY(QString filePath MEMBER filePath)
    Q_PROPERTY(QString fileFormat MEMBER fileFormat)
    Q_PROPERTY(QString toolMode MEMBER toolMode)

public:
    bool open = false;
    bool modified = false;
    bool pixelWritable = false;
    bool canSaveInPlace = false;
    bool liveStrokeActive = false;
    int strokeCount = 0;
    QString inputDevice = QStringLiteral("mouse");
    qreal inputPressure = 1.0;
    bool canUndo = false;
    bool canRedo = false;
    int bitmapWidth = 0;
    int bitmapHeight = 0;
    QString filePath;
    QString fileFormat;
    QString toolMode = QStringLiteral("brush");
};

Q_DECLARE_METATYPE(BitmapViewportConfig)
Q_DECLARE_METATYPE(BitmapRuntimeConfig)
Q_DECLARE_METATYPE(BitmapFileState)
