//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstdint>
#include <string>

#include "Core/PaintUuid.h"

struct DocumentMetadata {
    std::string title;
    std::string author;
    std::string storagePath;
    std::string createdAt;
    std::string modifiedAt;
    PaintUuid documentId;
    std::uint32_t version = 1;
    std::string appVersion;
};
