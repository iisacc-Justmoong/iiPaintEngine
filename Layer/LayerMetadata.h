//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <string>

#include "Core/PaintUuid.h"
#include "Core/RasterBlendMode.h"
#include "Core/Types.h"

struct LayerMetadata {
    PaintUuid id;
    std::string name;
    bool visible = true;
    Types::Scalar opacity = 1.0;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
};
