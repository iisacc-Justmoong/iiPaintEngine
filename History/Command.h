//
// Created by Justmoong on 2026 May 24.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/PaintRect.h"
#include "Core/PaintUuid.h"
#include "Core/Types.h"

enum class CommandKind {
    Unknown,
    PaintStroke,
    RasterEdit,
    LayerCreate,
    LayerDelete,
    LayerReorder,
    LayerMetadataChange,
    LayerMaskEdit,
    SelectionChange,
    Transform,
    BrushPresetChange,
    ColorChange,
    HistoryGroup,
};

enum class CommandScope {
    Document,
    Canvas,
    Layer,
    Mask,
    Stroke,
    Selection,
    Brush,
    Asset,
    HistoryGroup,
};

enum class CommandPayloadStorage {
    None,
    InlineBytes,
    AssetReference,
};

struct CommandPayload {
    CommandPayloadStorage storage = CommandPayloadStorage::None;
    std::string mimeType;
    PaintUuid assetId;
    std::vector<std::byte> bytes;
};

struct CommandPatch {
    PaintUuid targetId;
    CommandScope scope = CommandScope::Document;
    CommandPayload beforeState;
    CommandPayload afterState;
    DocumentRect dirtyBounds{};
};

struct Command {
    std::uint64_t sequence = 0;
    std::string label;
    PaintUuid targetId;
    CommandKind kind = CommandKind::Unknown;
    CommandScope scope = CommandScope::Document;
    PaintUuid transactionId;
    Types::Scalar timestamp = 0.0;
    std::string coalescingKey;
    bool reversible = true;
    bool committed = true;
    DocumentRect dirtyBounds{};
    std::vector<CommandPatch> patches;
};
