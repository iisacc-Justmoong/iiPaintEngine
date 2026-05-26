//
// Created by Justmoong on 2026 May 24.
//

#include "HistoryStack.h"

namespace {

void trimUndoDepth(HistoryStack &history)
{
    if (history.maxUndoCommands == 0) {
        return;
    }

    while (history.undoCommands.size() > history.maxUndoCommands) {
        history.undoCommands.erase(history.undoCommands.begin());
    }
    history.cursor = history.undoCommands.size();
}

} // namespace

bool canUndo(const HistoryStack &history)
{
    return !history.undoCommands.empty();
}

bool canRedo(const HistoryStack &history)
{
    return !history.redoCommands.empty();
}

bool recordHistoryCommand(HistoryStack &history, Command command)
{
    if (!command.reversible) {
        return false;
    }

    history.redoCommands.clear();
    if (command.sequence == 0) {
        command.sequence = history.nextSequence;
        ++history.nextSequence;
    } else if (command.sequence >= history.nextSequence) {
        history.nextSequence = command.sequence + 1;
    }

    history.undoCommands.push_back(command);
    trimUndoDepth(history);
    history.cursor = history.undoCommands.size();
    return true;
}

HistoryStepResult undoHistoryCommand(HistoryStack &history)
{
    HistoryStepResult result;
    if (!canUndo(history)) {
        return result;
    }

    result.applied = true;
    result.direction = HistoryStepDirection::Undo;
    result.command = history.undoCommands.back();
    history.undoCommands.pop_back();
    history.redoCommands.push_back(result.command);
    history.cursor = history.undoCommands.size();
    return result;
}

HistoryStepResult redoHistoryCommand(HistoryStack &history)
{
    HistoryStepResult result;
    if (!canRedo(history)) {
        return result;
    }

    result.applied = true;
    result.direction = HistoryStepDirection::Redo;
    result.command = history.redoCommands.back();
    history.redoCommands.pop_back();
    history.undoCommands.push_back(result.command);
    trimUndoDepth(history);
    history.cursor = history.undoCommands.size();
    return result;
}

void clearHistory(HistoryStack &history)
{
    history.undoCommands.clear();
    history.redoCommands.clear();
    history.cursor = 0;
    history.nextSequence = 1;
}
