//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <string>
#include <vector>

#include "Core/PaintUuid.h"
#include "Core/RasterBlendMode.h"
#include "Core/Types.h"

enum class LayerKind {
    Paint,
    Group,
    Adjustment,
};

struct LayerMask {
    bool enabled = false;
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    std::vector<Types::Byte> alpha;
};

struct LayerMetadata {
    PaintUuid id;
    std::string name;
    bool visible = true;
    Types::Scalar opacity = 1.0;
    RasterBlendMode blendMode = RasterBlendMode::SourceOver;
    LayerKind kind = LayerKind::Paint;
    bool clipsToBelow = false;
    bool alphaLock = false;
};
