//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <vector>

#include "History/Command.h"

struct HistoryStack {
    std::vector<Command> undoCommands;
    std::vector<Command> redoCommands;
    std::size_t cursor = 0;
    std::uint64_t nextSequence = 1;
    std::size_t maxUndoCommands = 0;
};

enum class HistoryStepDirection {
    None,
    Undo,
    Redo,
};

struct HistoryStepResult {
    bool applied = false;
    HistoryStepDirection direction = HistoryStepDirection::None;
    Command command;
};

bool canUndo(const HistoryStack &history);

bool canRedo(const HistoryStack &history);

bool recordHistoryCommand(HistoryStack &history, Command command);

HistoryStepResult undoHistoryCommand(HistoryStack &history);

HistoryStepResult redoHistoryCommand(HistoryStack &history);

void clearHistory(HistoryStack &history);
