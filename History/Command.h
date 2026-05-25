//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <string>

#include "Core/PaintUuid.h"

struct Command {
    std::uint64_t sequence = 0;
    std::string label;
    PaintUuid targetId;
};
