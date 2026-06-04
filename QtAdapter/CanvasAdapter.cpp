//
// Created by Justmoong on 2026 Jun 04.
//

#include "CanvasAdapter.h"

#include <QImage>
#include <QUrl>

namespace {

QString localFilePath(const QString &filePath)
{
    const QUrl url(filePath);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    return filePath;
}

QString normalizedToolMode(const QString &mode)
{
    const QString trimmed = mode.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("brush");
    }
    return trimmed;
}

} // namespace

CanvasAdapter::CanvasAdapter(QQuickItem *parent)
    : PaintCanvasItem(parent)
{
    connect(this, &PaintCanvasItem::strokeCountChanged, this, &CanvasAdapter::undoRedoChanged);
}

QString CanvasAdapter::toolMode() const
{
    return m_toolMode;
}

void CanvasAdapter::setToolMode(const QString &mode)
{
    const QString nextMode = normalizedToolMode(mode);
    if (m_toolMode == nextMode) {
        return;
    }

    m_toolMode = nextMode;
    emit toolModeChanged();
}

bool CanvasAdapter::canUndo() const
{
    return canUndoRasterChange();
}

bool CanvasAdapter::canRedo() const
{
    return canRedoRasterChange();
}

bool CanvasAdapter::newCanvas(int width, int height)
{
    const bool created = resetRasterCanvas(width, height);
    if (created) {
        emit undoRedoChanged();
    }
    return created;
}

bool CanvasAdapter::openRaster(const QString &filePath)
{
    const QImage image(localFilePath(filePath));
    const bool opened = replaceRasterCanvas(image);
    if (opened) {
        emit undoRedoChanged();
    }
    return opened;
}

bool CanvasAdapter::saveToFile(const QString &filePath)
{
    return saveRasterCanvasToFile(localFilePath(filePath));
}

bool CanvasAdapter::undo()
{
    const bool applied = undoRasterChange();
    if (applied) {
        emit undoRedoChanged();
    }
    return applied;
}

bool CanvasAdapter::redo()
{
    const bool applied = redoRasterChange();
    if (applied) {
        emit undoRedoChanged();
    }
    return applied;
}
