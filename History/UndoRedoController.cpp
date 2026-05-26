//
// Created by Justmoong on 2026 May 24.
//

#include "UndoRedoController.h"

bool recordHistoryCommand(UndoRedoController &controller, Command command)
{
    return recordHistoryCommand(controller.history, command);
}

HistoryStepResult undoHistoryCommand(UndoRedoController &controller)
{
    return undoHistoryCommand(controller.history);
}

HistoryStepResult redoHistoryCommand(UndoRedoController &controller)
{
    return redoHistoryCommand(controller.history);
}
