//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Brush/BrushSnapshot.h"
#include "Color/ColorSpace.h"
#include "Document/PaintDocument.h"
#include "History/HistoryStack.h"

inline constexpr std::uint32_t documentArchiveFormatVersion = 3;

struct DocumentAsset {
    PaintUuid id;
    std::string name;
    std::string mimeType;
    std::vector<std::byte> bytes;
};

struct DocumentArchive {
    std::string formatMagic = "iiPaintDocument";
    std::uint32_t formatVersion = documentArchiveFormatVersion;
    bool compatible = true;
    std::string compatibilityError;
    PaintDocument document;
    std::vector<BrushSnapshot> brushSources;
    std::vector<ColorSpace> colorSpaces;
    std::vector<DocumentAsset> assets;
    HistoryStack history;
};

struct DocumentSerializer {
    std::string formatMagic = "iiPaintDocument";
    std::uint32_t formatVersion = documentArchiveFormatVersion;
};

DocumentArchive makeDocumentArchive(const PaintDocument &document);

std::string serializeDocumentArchive(const DocumentArchive &archive);

DocumentArchive deserializeDocumentArchive(const std::string &payload);
