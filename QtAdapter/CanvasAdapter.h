//
// Created by Justmoong on 2026 Jun 04.
//

#pragma once

#include <QString>

#include "QtAdapter/PaintCanvasItem.h"

class CanvasAdapter : public PaintCanvasItem {
    Q_OBJECT
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)

public:
    explicit CanvasAdapter(QQuickItem *parent = nullptr);

    QString toolMode() const;
    void setToolMode(const QString &mode);

    bool canUndo() const;
    bool canRedo() const;

    Q_INVOKABLE bool newCanvas(int width, int height);
    Q_INVOKABLE bool openRaster(const QString &filePath);
    Q_INVOKABLE bool saveToFile(const QString &filePath);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();

signals:
    void toolModeChanged();
    void undoRedoChanged();

private:
    QString m_toolMode = QStringLiteral("brush");
};
