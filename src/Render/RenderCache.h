//
// Created by Justmoong on 2026 May 26.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Core/PaintUuid.h"
#include "Core/RasterSample.h"
#include "Layer/RasterLayer.h"
#include "Render/CpuRenderer.h"
#include "Render/DirtyRegion.h"
#include "Render/GpuRenderer.h"
#include "Render/RenderContext.h"

struct RenderTileKey {
    Types::Pixel tileX = 0;
    Types::Pixel tileY = 0;
    Types::Pixel tileSize = 256;
    std::uint64_t contentRevision = 0;
};

struct RenderTile {
    RenderTileKey key;
    DevicePixelRect rect{};
    RasterLayer layer;
    bool valid = false;
    std::uint64_t lastUsedFrame = 0;
};

struct RenderTileCache {
    bool enabled = false;
    Types::Pixel tileSize = 256;
    std::size_t capacity = 256;
    std::uint64_t contentRevision = 0;
    std::vector<RenderTile> tiles;
};

struct BrushStampAtlasKey {
    PaintUuid brushId;
    std::uint64_t revision = 0;
    Types::Pixel width = 0;
    Types::Pixel height = 0;
    RenderBufferFormat bufferFormat = RenderBufferFormat::UInt8;
};

struct BrushStampAtlasEntry {
    BrushStampAtlasKey key;
    std::vector<Types::Byte> alpha;
    bool valid = false;
    std::uint64_t lastUsedFrame = 0;
};

struct BrushStampAtlas {
    bool enabled = false;
    std::size_t capacity = 256;
    std::vector<BrushStampAtlasEntry> entries;
};

std::vector<DevicePixelRect> tileRectsForDirtyRegion(DevicePixelRect documentBounds,
                                                     const DirtyRegion &dirtyRegion,
                                                     Types::Pixel tileSize);

void storeRenderTile(RenderTileCache &cache,
                     DevicePixelRect rect,
                     const RasterLayer &layer,
                     std::uint64_t frameIndex = 0);

const RenderTile *findRenderTile(const RenderTileCache &cache,
                                 DevicePixelPoint position,
                                 std::uint64_t contentRevision);

void invalidateRenderTiles(RenderTileCache &cache, const DirtyRegion &dirtyRegion);

void storeBrushStamp(BrushStampAtlas &atlas,
                     const BrushStampAtlasKey &key,
                     const std::vector<Types::Byte> &alpha,
                     std::uint64_t frameIndex = 0);

const BrushStampAtlasEntry *findBrushStamp(const BrushStampAtlas &atlas,
                                           const BrushStampAtlasKey &key);

RenderExecutionPlan resolveRenderExecutionPlan(const RenderContext &context,
                                               const CpuRenderer &cpu,
                                               const GpuRenderer &gpu);
