//
// Created by Justmoong on 2026 Jun 04.
//

#pragma once

#include <QString>
#include <QStringList>

#include "QtAdapter/CanvasApiConfig.h"
#include "QtAdapter/CanvasBrushConfig.h"
#include "QtAdapter/PaintCanvasItem.h"

class CanvasAdapter : public PaintCanvasItem {
    Q_OBJECT
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(CanvasBrushConfig brushConfig READ brushConfig WRITE setBrushConfig NOTIFY brushConfigChanged)
    Q_PROPERTY(CanvasViewportConfig viewportConfig READ viewportConfig WRITE setViewportConfig NOTIFY viewportConfigChanged)
    Q_PROPERTY(CanvasRuntimeConfig runtimeConfig READ runtimeConfig WRITE setRuntimeConfig NOTIFY runtimeConfigChanged)
    Q_PROPERTY(CanvasStateSnapshot stateSnapshot READ stateSnapshot NOTIFY stateSnapshotChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)
    Q_PROPERTY(QStringList supportedOpenFormats READ supportedOpenFormats CONSTANT)
    Q_PROPERTY(QStringList supportedSaveFormats READ supportedSaveFormats CONSTANT)
    Q_PROPERTY(QString lastFileError READ lastFileError NOTIFY lastFileErrorChanged)

public:
    explicit CanvasAdapter(QQuickItem *parent = nullptr);

    QString toolMode() const;
    void setToolMode(const QString &mode);

    CanvasBrushConfig brushConfig() const;
    Q_INVOKABLE void setBrushConfig(const CanvasBrushConfig &config);

    CanvasViewportConfig viewportConfig() const;
    Q_INVOKABLE void setViewportConfig(const CanvasViewportConfig &config);

    CanvasRuntimeConfig runtimeConfig() const;
    Q_INVOKABLE void setRuntimeConfig(const CanvasRuntimeConfig &config);

    CanvasStateSnapshot stateSnapshot() const;

    bool canUndo() const;
    bool canRedo() const;
    QStringList supportedOpenFormats() const;
    QStringList supportedSaveFormats() const;
    QString lastFileError() const;

    Q_INVOKABLE bool newCanvas(int width, int height);
    Q_INVOKABLE bool openRaster(const QString &filePath);
    Q_INVOKABLE bool saveToFile(const QString &filePath);
    Q_INVOKABLE bool saveToFileAs(const QString &filePath, const QString &format, int quality = -1);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();

signals:
    void toolModeChanged();
    void brushConfigChanged();
    void viewportConfigChanged();
    void runtimeConfigChanged();
    void stateSnapshotChanged();
    void undoRedoChanged();
    void lastFileErrorChanged();

private:
    QString m_toolMode = QStringLiteral("brush");
    QString m_lastFileError;

    void setLastFileError(const QString &error);
};
