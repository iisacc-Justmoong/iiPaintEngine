//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include "History/HistoryStack.h"

struct UndoRedoController {
    HistoryStack history;
};

bool recordHistoryCommand(UndoRedoController &controller, Command command);

HistoryStepResult undoHistoryCommand(UndoRedoController &controller);

HistoryStepResult redoHistoryCommand(UndoRedoController &controller);
