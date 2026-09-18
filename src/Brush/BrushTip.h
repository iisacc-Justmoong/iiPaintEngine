//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <vector>

struct BrushTip {
    int width = 0;
    int height = 0;
    std::vector<std::byte> mask;
};
