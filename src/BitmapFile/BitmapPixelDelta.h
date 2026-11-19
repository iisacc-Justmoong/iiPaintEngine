#pragma once

#include "Core/EngineError.h"
#include "Core/PaintRect.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>

struct RasterLayer;

// An immutable ARGB32 XOR patch. The same patch reverses or reapplies the edit.
class BitmapPixelDelta final {
public:
    // Encode a dirty rectangle between matching raster layers. Charge the entry and payload to maximumBytes.
    static std::expected<BitmapPixelDelta, EngineErrorCode> between(const RasterLayer &before,
                                                                   const RasterLayer &after,
                                                                   DevicePixelRect bounds,
                                                                   std::size_t maximumBytes);
    // Encode a solid-color edit directly, with no destination frame or uncompressed staging buffer.
    static std::expected<BitmapPixelDelta, EngineErrorCode> forFill(const RasterLayer &before,
                                                                   std::uint32_t argb,
                                                                   std::size_t maximumBytes);

    [[nodiscard]] bool empty() const noexcept { return !m_words; }
    [[nodiscard]] DevicePixelRect bounds() const noexcept { return m_bounds; }
    // Charge each history entry and its exact payload size, excluding allocator bookkeeping.
    [[nodiscard]] std::size_t retainedBytes() const noexcept;

private:
    friend class BitmapFile;
    template <typename Replacement>
    static std::expected<BitmapPixelDelta, EngineErrorCode> encode(const RasterLayer &before,
                                                                  DevicePixelRect bounds,
                                                                  std::size_t maximumBytes,
                                                                  Replacement replacement);
    // Validate the destination once, then apply without allocating or reading a full pixel snapshot.
    bool applyTo(RasterLayer &destination) const noexcept;

    DevicePixelRect m_bounds{};
    std::shared_ptr<const std::uint32_t[]> m_words;
    std::size_t m_wordCount = 0;
    bool m_runLength = false;
};
