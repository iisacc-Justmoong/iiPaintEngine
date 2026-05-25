//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct ColorSpace {
    std::string name = "sRGB";
    std::vector<std::byte> iccProfile;
    bool linear = false;
};
