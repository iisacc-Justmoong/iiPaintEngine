//
// Created by Justmoong on 2026 May 26.
//

#include "RenderCache.h"

#include <algorithm>

namespace {

bool uuidEquals(const PaintUuid &lhs, const PaintUuid &rhs)
{
    return lhs.bytes == rhs.bytes;
}

bool rectsEqual(DevicePixelRect lhs, DevicePixelRect rhs)
{
    return lhs.origin.x == rhs.origin.x
            && lhs.origin.y == rhs.origin.y
            && lhs.width == rhs.width
            && lhs.height == rhs.height;
}

bool contains(DevicePixelRect rect, DevicePixelPoint position)
{
    return !isEmpty(rect)
            && position.x >= rect.origin.x
            && position.y >= rect.origin.y
            && position.x < rect.origin.x + rect.width
            && position.y < rect.origin.y + rect.height;
}

bool intersects(DevicePixelRect lhs, DevicePixelRect rhs)
{
    return !isEmpty(intersectDevicePixelRects(lhs, rhs));
}

Types::Pixel normalizedTileSize(Types::Pixel tileSize)
{
    return std::max<Types::Pixel>(1, tileSize);
}

Types::Pixel floorToTile(Types::Pixel value, Types::Pixel tileSize)
{
    if (value >= 0) {
        return (value / tileSize) * tileSize;
    }
    return -(((-value + tileSize - 1) / tileSize) * tileSize);
}

void trimRenderTileCache(RenderTileCache &cache)
{
    if (cache.capacity == 0) {
        cache.tiles.clear();
        return;
    }

    while (cache.tiles.size() > cache.capacity) {
        const auto oldest = std::min_element(cache.tiles.begin(),
                                             cache.tiles.end(),
                                             [](const RenderTile &lhs, const RenderTile &rhs) {
                                                 return lhs.lastUsedFrame < rhs.lastUsedFrame;
                                             });
        if (oldest == cache.tiles.end()) {
            return;
        }
        cache.tiles.erase(oldest);
    }
}

void trimBrushStampAtlas(BrushStampAtlas &atlas)
{
    if (atlas.capacity == 0) {
        atlas.entries.clear();
        return;
    }

    while (atlas.entries.size() > atlas.capacity) {
        const auto oldest = std::min_element(atlas.entries.begin(),
                                             atlas.entries.end(),
                                             [](const BrushStampAtlasEntry &lhs, const BrushStampAtlasEntry &rhs) {
                                                 return lhs.lastUsedFrame < rhs.lastUsedFrame;
                                             });
        if (oldest == atlas.entries.end()) {
            return;
        }
        atlas.entries.erase(oldest);
    }
}

void trimStrokeReplayCache(StrokeReplayCache &cache)
{
    if (cache.capacity == 0) {
        cache.entries.clear();
        return;
    }

    while (cache.entries.size() > cache.capacity) {
        const auto oldest = std::min_element(cache.entries.begin(),
                                             cache.entries.end(),
                                             [](const StrokeReplayCacheEntry &lhs, const StrokeReplayCacheEntry &rhs) {
                                                 return lhs.lastUsedFrame < rhs.lastUsedFrame;
                                             });
        if (oldest == cache.entries.end()) {
            return;
        }
        cache.entries.erase(oldest);
    }
}

bool brushStampKeysEqual(const BrushStampAtlasKey &lhs, const BrushStampAtlasKey &rhs)
{
    return uuidEquals(lhs.brushId, rhs.brushId)
            && lhs.revision == rhs.revision
            && lhs.width == rhs.width
            && lhs.height == rhs.height
            && lhs.bufferFormat == rhs.bufferFormat;
}

bool strokeReplayKeysEqual(const StrokeReplayCacheKey &lhs, const StrokeReplayCacheKey &rhs)
{
    return uuidEquals(lhs.strokeId, rhs.strokeId)
            && lhs.brushRevision == rhs.brushRevision
            && lhs.strokeRevision == rhs.strokeRevision
            && lhs.bufferFormat == rhs.bufferFormat
            && lhs.tileX == rhs.tileX
            && lhs.tileY == rhs.tileY;
}

} // namespace

std::vector<DevicePixelRect> tileRectsForDirtyRegion(DevicePixelRect canvasBounds,
                                                     const DirtyRegion &dirtyRegion,
                                                     Types::Pixel tileSize)
{
    std::vector<DevicePixelRect> tiles;
    const Types::Pixel size = normalizedTileSize(tileSize);
    if (isEmpty(canvasBounds)) {
        return tiles;
    }

    for (const DevicePixelRect dirtyRect : dirtyRegion.rects) {
        const DevicePixelRect clippedDirty = intersectDevicePixelRects(canvasBounds, dirtyRect);
        if (isEmpty(clippedDirty)) {
            continue;
        }

        const Types::Pixel startX = floorToTile(clippedDirty.origin.x, size);
        const Types::Pixel startY = floorToTile(clippedDirty.origin.y, size);
        const Types::Pixel endX = floorToTile(clippedDirty.origin.x + clippedDirty.width - 1, size);
        const Types::Pixel endY = floorToTile(clippedDirty.origin.y + clippedDirty.height - 1, size);
        for (Types::Pixel y = startY; y <= endY; y += size) {
            for (Types::Pixel x = startX; x <= endX; x += size) {
                const DevicePixelRect tile = intersectDevicePixelRects(canvasBounds, DevicePixelRect{{x, y}, size, size});
                if (isEmpty(tile)) {
                    continue;
                }

                const bool exists = std::any_of(tiles.begin(), tiles.end(), [tile](DevicePixelRect existing) {
                    return rectsEqual(existing, tile);
                });
                if (!exists) {
                    tiles.push_back(tile);
                }
            }
        }
    }

    return tiles;
}

void storeRenderTile(RenderTileCache &cache,
                     DevicePixelRect rect,
                     const RasterLayer &layer,
                     std::uint64_t frameIndex)
{
    if (!cache.enabled || isEmpty(rect)) {
        return;
    }

    const Types::Pixel size = normalizedTileSize(cache.tileSize);
    RenderTile tile;
    tile.key = RenderTileKey{rect.origin.x, rect.origin.y, size, cache.contentRevision};
    tile.rect = rect;
    tile.layer = layer;
    tile.valid = true;
    tile.lastUsedFrame = frameIndex;

    for (RenderTile &entry : cache.tiles) {
        if (entry.key.tileX == tile.key.tileX
                && entry.key.tileY == tile.key.tileY
                && entry.key.tileSize == tile.key.tileSize
                && entry.key.contentRevision == tile.key.contentRevision) {
            entry = tile;
            trimRenderTileCache(cache);
            return;
        }
    }

    cache.tiles.push_back(tile);
    trimRenderTileCache(cache);
}

const RenderTile *findRenderTile(const RenderTileCache &cache,
                                 DevicePixelPoint position,
                                 std::uint64_t contentRevision)
{
    if (!cache.enabled) {
        return nullptr;
    }

    for (const RenderTile &tile : cache.tiles) {
        if (tile.valid
                && tile.key.contentRevision == contentRevision
                && contains(tile.rect, position)) {
            return &tile;
        }
    }
    return nullptr;
}

void invalidateRenderTiles(RenderTileCache &cache, const DirtyRegion &dirtyRegion)
{
    for (RenderTile &tile : cache.tiles) {
        if (!tile.valid) {
            continue;
        }
        for (const DevicePixelRect dirtyRect : dirtyRegion.rects) {
            if (intersects(tile.rect, dirtyRect)) {
                tile.valid = false;
                break;
            }
        }
    }
}

void storeBrushStamp(BrushStampAtlas &atlas,
                     const BrushStampAtlasKey &key,
                     const std::vector<Types::Byte> &alpha,
                     std::uint64_t frameIndex)
{
    if (!atlas.enabled) {
        return;
    }

    BrushStampAtlasEntry entry;
    entry.key = key;
    entry.alpha = alpha;
    entry.valid = true;
    entry.lastUsedFrame = frameIndex;

    for (BrushStampAtlasEntry &existing : atlas.entries) {
        if (brushStampKeysEqual(existing.key, key)) {
            existing = entry;
            trimBrushStampAtlas(atlas);
            return;
        }
    }

    atlas.entries.push_back(entry);
    trimBrushStampAtlas(atlas);
}

const BrushStampAtlasEntry *findBrushStamp(const BrushStampAtlas &atlas,
                                           const BrushStampAtlasKey &key)
{
    if (!atlas.enabled) {
        return nullptr;
    }

    for (const BrushStampAtlasEntry &entry : atlas.entries) {
        if (entry.valid && brushStampKeysEqual(entry.key, key)) {
            return &entry;
        }
    }
    return nullptr;
}

void storeStrokeReplay(StrokeReplayCache &cache,
                       const StrokeReplayCacheKey &key,
                       const std::vector<RasterSample> &samples,
                       DevicePixelRect dirtyBounds,
                       std::uint64_t frameIndex)
{
    if (!cache.enabled) {
        return;
    }

    StrokeReplayCacheEntry entry;
    entry.key = key;
    entry.samples = samples;
    entry.dirtyBounds = dirtyBounds;
    entry.valid = true;
    entry.lastUsedFrame = frameIndex;

    for (StrokeReplayCacheEntry &existing : cache.entries) {
        if (strokeReplayKeysEqual(existing.key, key)) {
            existing = entry;
            trimStrokeReplayCache(cache);
            return;
        }
    }

    cache.entries.push_back(entry);
    trimStrokeReplayCache(cache);
}

const StrokeReplayCacheEntry *findStrokeReplay(const StrokeReplayCache &cache,
                                               const StrokeReplayCacheKey &key)
{
    if (!cache.enabled) {
        return nullptr;
    }

    for (const StrokeReplayCacheEntry &entry : cache.entries) {
        if (entry.valid && strokeReplayKeysEqual(entry.key, key)) {
            return &entry;
        }
    }
    return nullptr;
}

void invalidateStrokeReplayTiles(StrokeReplayCache &cache, const DirtyRegion &dirtyRegion)
{
    for (StrokeReplayCacheEntry &entry : cache.entries) {
        if (!entry.valid) {
            continue;
        }
        for (const DevicePixelRect dirtyRect : dirtyRegion.rects) {
            if (intersects(entry.dirtyBounds, dirtyRect)) {
                entry.valid = false;
                break;
            }
        }
    }
}

RenderExecutionPlan resolveRenderExecutionPlan(const RenderContext &context,
                                               const CpuRenderer &cpu,
                                               const GpuRenderer &gpu)
{
    RenderExecutionPlan plan;
    plan.backend = context.preferredBackend;
    plan.bufferFormat = renderBufferFormatForColorSpace(context.targetColorSpace);
    plan.usesTileCache = context.tileCacheEnabled;
    plan.usesStrokeReplayCache = context.strokeReplayCacheEnabled;
    plan.linearCompositing = context.linearCompositingEnabled
            && renderColorSpaceRequiresLinearCompositing(context.targetColorSpace);
    plan.wideGamut = colorSpaceSupportsWideGamut(context.targetColorSpace);
    plan.hdr = colorSpaceSupportsHdr(context.targetColorSpace);
    plan.iccManaged = !context.targetColorSpace.iccProfile.empty();

    const bool wantsGpu = context.preferredBackend == RenderBackend::Gpu;
    const bool canUseGpu = wantsGpu && context.gpuPathEnabled && gpu.available;
    if (wantsGpu && !canUseGpu && context.allowCpuFallback) {
        plan.backend = RenderBackend::Cpu;
        plan.usedCpuFallback = true;
    }

    if (plan.backend == RenderBackend::Gpu && canUseGpu && gpu.computeAvailable) {
        plan.accelerationPath = RenderAccelerationPath::GpuCompute;
        plan.usesBrushStampAtlas = context.brushStampAtlasEnabled && gpu.brushStampAtlasSupported;
    } else if (context.simdEnabled && cpu.available && cpu.simdAvailable) {
        plan.accelerationPath = RenderAccelerationPath::SimdCpu;
        plan.usesBrushStampAtlas = context.brushStampAtlasEnabled;
    } else {
        plan.accelerationPath = RenderAccelerationPath::ScalarCpu;
        plan.usesBrushStampAtlas = context.brushStampAtlasEnabled && plan.backend == RenderBackend::Cpu;
    }

    return plan;
}
