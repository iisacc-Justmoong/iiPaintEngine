//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/PaintRect.h"
#include "Core/PaintUuid.h"
#include "Core/Types.h"

enum class CanvasUnit {
    Pixel,
    Inch,
    Millimeter,
};

struct CanvasMetadata {
    std::string title;
    std::string author;
    std::string createdAt;
    std::string modifiedAt;
    PaintUuid documentId;
    std::vector<std::uint32_t> thumbnail;
    std::uint32_t backgroundColor = 0x00000000U;
    CanvasUnit unit = CanvasUnit::Pixel;
    Types::Pixel intendedExportWidth = 0;
    Types::Pixel intendedExportHeight = 0;
    Types::Scalar dpiX = 72.0;
    Types::Scalar dpiY = 72.0;
    std::string colorSpace = "sRGB";
    std::string appVersion;
};
