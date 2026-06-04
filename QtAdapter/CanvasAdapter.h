//
// Created by Justmoong on 2026 Jun 04.
//

#pragma once

#include <QString>

#include "QtAdapter/CanvasBrushConfig.h"
#include "QtAdapter/PaintCanvasItem.h"

class CanvasAdapter : public PaintCanvasItem {
    Q_OBJECT
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(CanvasBrushConfig brushConfig READ brushConfig WRITE setBrushConfig NOTIFY brushConfigChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)

public:
    explicit CanvasAdapter(QQuickItem *parent = nullptr);

    QString toolMode() const;
    void setToolMode(const QString &mode);

    CanvasBrushConfig brushConfig() const;
    Q_INVOKABLE void setBrushConfig(const CanvasBrushConfig &config);

    bool canUndo() const;
    bool canRedo() const;

    Q_INVOKABLE bool newCanvas(int width, int height);
    Q_INVOKABLE bool openRaster(const QString &filePath);
    Q_INVOKABLE bool saveToFile(const QString &filePath);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();

signals:
    void toolModeChanged();
    void brushConfigChanged();
    void undoRedoChanged();

private:
    QString m_toolMode = QStringLiteral("brush");
};
