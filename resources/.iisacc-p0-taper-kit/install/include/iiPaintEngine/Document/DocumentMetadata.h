//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/PaintUuid.h"
#include "Core/Types.h"

enum class DocumentUnit {
    Pixel,
    Inch,
    Millimeter,
};

struct DocumentMetadata {
    std::string title;
    std::string author;
    std::string storagePath;
    std::string createdAt;
    std::string modifiedAt;
    PaintUuid documentId;
    std::uint32_t version = 1;
    std::vector<std::uint32_t> thumbnail;
    std::uint32_t backgroundColor = 0x00000000U;
    DocumentUnit unit = DocumentUnit::Pixel;
    Types::Pixel intendedExportWidth = 0;
    Types::Pixel intendedExportHeight = 0;
    Types::Scalar dpiX = 72.0;
    Types::Scalar dpiY = 72.0;
    std::string colorSpace = "sRGB";
    std::string appVersion;
};
