//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "Brush/BrushSnapshot.h"
#include "Color/ColorSpace.h"
#include "Document/PaintDocument.h"
#include "History/HistoryStack.h"

inline constexpr std::uint32_t documentArchiveFormatVersion = 4;

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

// Total collection limits for one read. A caller can lower the defaults for smaller memory budgets.
struct DocumentArchiveReadLimits {
    std::size_t maxLayers = 10'000;            // Top-level layers and all descendants combined.
    std::size_t maxLayerDepth = 64;            // A top-level layer has depth 1.
    std::size_t maxDirtyRects = 1'000'000;    // Document surface and all layer surfaces combined.
    std::size_t maxHistoryCommands = 100'000; // Undo and redo combined.
    std::size_t maxHistoryPatches = 1'000'000; // All patches in both history branches combined.
    std::size_t maxBrushSources = 10'000;
    std::size_t maxColorSpaces = 1'024;
    std::size_t maxAssets = 100'000;
};

DocumentArchive makeDocumentArchive(const PaintDocument &document);

// 임시 문서의 표면과 레이어 버퍼를 아카이브로 이동한다.
DocumentArchive makeDocumentArchive(PaintDocument &&document);

// 길이로 구분한 바이너리 블록을 포함하는 버전 4 바이트열을 반환한다.
std::string serializeDocumentArchive(const DocumentArchive &archive);

// 호출자가 연 바이너리 스트림에 순서대로 기록한다. 쓰기 실패 시 false를 반환하며 이미 보낸 바이트는 스트림 소유자가 처리한다.
bool serializeDocumentArchiveTo(const DocumentArchive &archive, std::ostream &output);

// Reads with the default limits. A failed read returns compatible=false and no partial document.
DocumentArchive deserializeDocumentArchive(const std::string &payload);

// Reads with caller-specified limits, each capped at the corresponding default.
DocumentArchive deserializeDocumentArchiveWithLimits(const std::string &payload,
                                                     DocumentArchiveReadLimits limits);

// 동기 호출 동안 입력을 빌리며 반환한 문서는 복원된 픽셀과 메타데이터를 직접 소유한다.
DocumentArchive deserializeDocumentArchiveView(std::string_view payload, DocumentArchiveReadLimits limits = {});
