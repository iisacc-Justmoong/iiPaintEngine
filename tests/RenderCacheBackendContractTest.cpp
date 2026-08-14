#include <cstddef>
#include <cstdint>
#include <vector>

#include "Color/ColorSpace.h"
#include "Render/RenderCache.h"
#include "Render/Renderer.h"

namespace {

PaintUuid uuidWithFirstByte(std::uint8_t value)
{
    PaintUuid id;
    id.bytes[0] = value;
    return id;
}

bool sameRect(DevicePixelRect lhs, DevicePixelRect rhs)
{
    return lhs.origin.x == rhs.origin.x
            && lhs.origin.y == rhs.origin.y
            && lhs.width == rhs.width
            && lhs.height == rhs.height;
}

bool containsRect(const std::vector<DevicePixelRect> &rects, DevicePixelRect expected)
{
    for (const DevicePixelRect rect : rects) {
        if (sameRect(rect, expected)) {
            return true;
        }
    }
    return false;
}

} // namespace

int main()
{
    static_assert(RenderAccelerationPath::ScalarCpu != RenderAccelerationPath::SimdCpu);
    static_assert(RenderBufferFormat::UInt8 != RenderBufferFormat::Float32);

    const DevicePixelRect canvasBounds{{0, 0}, 512, 512};
    const DirtyRegion dirty = makeDirtyRegion({
            DevicePixelRect{{12, 12}, 1, 1},
            DevicePixelRect{{300, 4}, 20, 20},
            DevicePixelRect{{300, 4}, 20, 20},
    });
    const std::vector<DevicePixelRect> tileRects = tileRectsForDirtyRegion(canvasBounds, dirty, 128);
    if (tileRects.size() != 2
            || !containsRect(tileRects, DevicePixelRect{{0, 0}, 128, 128})
            || !containsRect(tileRects, DevicePixelRect{{256, 0}, 128, 128})) {
        return 1;
    }

    RenderTileCache tileCache;
    tileCache.enabled = true;
    tileCache.tileSize = 128;
    tileCache.capacity = 2;
    tileCache.contentRevision = 9;
    storeRenderTile(tileCache, DevicePixelRect{{0, 0}, 128, 128}, makeRasterLayer(128, 128, 0xFF102030U), 1);
    storeRenderTile(tileCache, DevicePixelRect{{256, 0}, 128, 128}, makeRasterLayer(128, 128, 0xFF405060U), 2);
    if (findRenderTile(tileCache, DevicePixelPoint{0, 0}, 9) == nullptr
            || findRenderTile(tileCache, DevicePixelPoint{300, 12}, 9) == nullptr
            || findRenderTile(tileCache, DevicePixelPoint{0, 0}, 10) != nullptr) {
        return 2;
    }

    invalidateRenderTiles(tileCache, makeDirtyRegion({DevicePixelRect{{0, 0}, 1, 1}}));
    if (findRenderTile(tileCache, DevicePixelPoint{0, 0}, 9) != nullptr
            || findRenderTile(tileCache, DevicePixelPoint{300, 12}, 9) == nullptr) {
        return 3;
    }

    BrushStampAtlas atlas;
    atlas.enabled = true;
    atlas.capacity = 1;
    BrushStampAtlasKey firstStamp{uuidWithFirstByte(1), 3, 16, 16, RenderBufferFormat::Float16};
    BrushStampAtlasKey secondStamp{uuidWithFirstByte(2), 4, 32, 32, RenderBufferFormat::Float16};
    storeBrushStamp(atlas, firstStamp, {0, 128, 255}, 1);
    if (findBrushStamp(atlas, firstStamp) == nullptr) {
        return 4;
    }
    storeBrushStamp(atlas, secondStamp, {255}, 2);
    if (findBrushStamp(atlas, firstStamp) != nullptr
            || findBrushStamp(atlas, secondStamp) == nullptr) {
        return 5;
    }

    ColorSpace p3Float = makeDisplayP3LinearFloatColorSpace();
    p3Float.iccProfile = {std::byte{0x01}, std::byte{0x02}};

    Renderer renderer;
    renderer.context.sourceColorSpace = p3Float;
    renderer.context.targetColorSpace = p3Float;
    renderer.context.preferredBackend = RenderBackend::Gpu;
    renderer.context.allowCpuFallback = true;
    renderer.context.tileCacheEnabled = true;
    renderer.context.brushStampAtlasEnabled = true;
    renderer.cpu.simdAvailable = true;
    renderer.gpu.available = false;

    const RenderExecutionPlan cpuFallbackPlan = resolveRenderExecutionPlan(renderer.context, renderer.cpu, renderer.gpu);
    if (cpuFallbackPlan.backend != RenderBackend::Cpu
            || !cpuFallbackPlan.usedCpuFallback
            || cpuFallbackPlan.accelerationPath != RenderAccelerationPath::SimdCpu
            || cpuFallbackPlan.bufferFormat != RenderBufferFormat::Float32
            || !cpuFallbackPlan.usesTileCache
            || !cpuFallbackPlan.usesBrushStampAtlas
            || !cpuFallbackPlan.linearCompositing
            || !cpuFallbackPlan.wideGamut
            || !cpuFallbackPlan.hdr
            || !cpuFallbackPlan.iccManaged) {
        return 9;
    }

    renderer.gpu.available = true;
    renderer.gpu.computeAvailable = true;
    renderer.gpu.brushStampAtlasSupported = true;
    const RenderExecutionPlan gpuPlan = resolveRenderExecutionPlan(renderer.context, renderer.cpu, renderer.gpu);
    if (gpuPlan.backend != RenderBackend::Gpu
            || gpuPlan.usedCpuFallback
            || gpuPlan.accelerationPath != RenderAccelerationPath::GpuCompute
            || !gpuPlan.usesBrushStampAtlas) {
        return 10;
    }

    return 0;
}
