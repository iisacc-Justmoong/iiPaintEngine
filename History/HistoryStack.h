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
};
