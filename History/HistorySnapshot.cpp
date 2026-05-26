//
// Created by Justmoong on 2026 May 24.
//

#include "HistorySnapshot.h"

HistorySnapshot makeHistorySnapshot(const HistoryStack &history)
{
    return HistorySnapshot{
            history.undoCommands.size(),
            history.redoCommands.size(),
            history.cursor,
            history.nextSequence,
            canUndo(history),
            canRedo(history),
    };
}
