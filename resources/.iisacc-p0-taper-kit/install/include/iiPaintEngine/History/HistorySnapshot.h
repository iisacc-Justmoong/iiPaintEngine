//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <cstdint>

#include "History/HistoryStack.h"

struct HistorySnapshot {
    std::size_t undoDepth = 0;
    std::size_t redoDepth = 0;
    std::size_t cursor = 0;
    std::uint64_t nextSequence = 1;
    bool canUndo = false;
    bool canRedo = false;
};

HistorySnapshot makeHistorySnapshot(const HistoryStack &history);
