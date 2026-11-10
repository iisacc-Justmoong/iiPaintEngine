#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>
#include "Brush/BrushPreset.h"

enum class AbrImportErrorCode {
    TruncatedData, UnsupportedVersion, UnsupportedContent, UnsupportedDepth,
    UnsupportedCompression, InvalidData, ResourceLimit
};
struct AbrImportError {
    AbrImportErrorCode code = AbrImportErrorCode::InvalidData;
    std::size_t offset = 0;
    std::string message;
};
struct AbrImportLimits {
    std::size_t maxFileBytes = 256 * 1024 * 1024;
    std::size_t maxBrushes = 4096;
    std::size_t maxTotalMaskBytes = 64 * 1024 * 1024;
};
struct AbrImportedBrush {
    BrushPreset preset;
    std::string sourceId;
    std::uint16_t sourceDepth = 0; // 0 for computed tips; 8/16 for sampled coverage.
    bool completeSettings = false;
};
struct AbrBrushImport {
    std::uint16_t version = 0;
    std::uint16_t subversion = 0;
    std::vector<AbrImportedBrush> brushes;
    std::vector<std::string> warnings;
};

// Atomic memory decode: v1/v2 geometry/spacing; v6/v7/v9/v10 sampled tips.
// Modern descriptors, patterns and folders are reported as unsupported.
std::expected<AbrBrushImport, AbrImportError> importAbrBrushes(
        std::span<const std::byte> bytes, const AbrImportLimits &limits = {});
