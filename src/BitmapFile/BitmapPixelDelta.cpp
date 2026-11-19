#include "BitmapPixelDelta.h"
#include "Layer/RasterLayer.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace {

// Validate the complete raster storage and a nonempty rectangle before indexing any of its rows.
bool containsPatch(const RasterLayer &layer, DevicePixelRect bounds) noexcept
{
    if (layer.width <= 0 || layer.height <= 0 || bounds.width <= 0 || bounds.height <= 0
            || bounds.origin.x < 0 || bounds.origin.y < 0
            || static_cast<std::int64_t>(bounds.origin.x) + bounds.width > layer.width
            || static_cast<std::int64_t>(bounds.origin.y) + bounds.height > layer.height) return false;
    const auto pixels = static_cast<std::uint64_t>(layer.width) * layer.height;
    return pixels <= std::numeric_limits<std::uint32_t>::max() && pixels == layer.pixels.size();
}

// Return the first pixel of a row in a validated patch.
std::size_t rowStart(const RasterLayer &layer, DevicePixelRect bounds, int row) noexcept
{
    return static_cast<std::size_t>(bounds.origin.y + row) * layer.width + bounds.origin.x;
}

} // namespace

template <typename Replacement>
std::expected<BitmapPixelDelta, EngineErrorCode> BitmapPixelDelta::encode(const RasterLayer &before,
                                                                        DevicePixelRect bounds,
                                                                        std::size_t maximumBytes,
                                                                        Replacement replacement)
{
    if (!containsPatch(before, bounds)) return std::unexpected(EngineErrorCode::InvalidArgument);

    // Count both representations before allocating. Runs can cross row boundaries.
    const auto scan = [&](auto visit) {
        for (int y = 0; y < bounds.height; ++y) {
            const auto start = rowStart(before, bounds, y);
            for (int x = 0; x < bounds.width; ++x) {
                const auto index = start + x;
                visit(before.pixels[index] ^ replacement(index));
            }
        }
    };
    std::size_t runs = 0;
    std::uint32_t previous = 0;
    bool changed = false;
    scan([&](std::uint32_t delta) {
        if (runs == 0 || delta != previous) ++runs;
        previous = delta;
        changed |= delta != 0;
    });
    if (!changed) return BitmapPixelDelta{};

    BitmapPixelDelta patch;
    patch.m_bounds = bounds;
    const auto pixels = static_cast<std::size_t>(bounds.width) * bounds.height;
    patch.m_runLength = runs <= (pixels - 1) / 2;
    patch.m_wordCount = patch.m_runLength ? runs * 2 : pixels;
    if (maximumBytes < sizeof(BitmapPixelDelta)
            || patch.m_wordCount > (maximumBytes - sizeof(BitmapPixelDelta)) / sizeof(std::uint32_t)) {
        return std::unexpected(EngineErrorCode::BudgetExceeded);
    }

    auto words = std::make_shared_for_overwrite<std::uint32_t[]>(patch.m_wordCount);
    std::size_t cursor = 0;
    if (patch.m_runLength) {
        scan([&](std::uint32_t delta) {
            if (cursor == 0 || words[cursor - 1] != delta) {
                words[cursor++] = 1;
                words[cursor++] = delta;
            } else {
                ++words[cursor - 2];
            }
        });
    } else {
        scan([&](std::uint32_t delta) { words[cursor++] = delta; });
    }
    patch.m_words = std::move(words);
    return patch;
}

std::expected<BitmapPixelDelta, EngineErrorCode> BitmapPixelDelta::between(const RasterLayer &before,
                                                                         const RasterLayer &after,
                                                                         DevicePixelRect bounds,
                                                                         std::size_t maximumBytes)
{
    if (before.width != after.width || before.height != after.height || !containsPatch(after, bounds)) {
        return std::unexpected(EngineErrorCode::InvalidArgument);
    }
    return encode(before, bounds, maximumBytes, [&](std::size_t index) { return after.pixels[index]; });
}

std::expected<BitmapPixelDelta, EngineErrorCode> BitmapPixelDelta::forFill(const RasterLayer &before,
                                                                         std::uint32_t argb,
                                                                         std::size_t maximumBytes)
{
    return encode(before, {{}, before.width, before.height}, maximumBytes, [argb](std::size_t) { return argb; });
}

std::size_t BitmapPixelDelta::retainedBytes() const noexcept
{
    return empty() ? 0 : sizeof(*this) + m_wordCount * sizeof(std::uint32_t);
}

bool BitmapPixelDelta::applyTo(RasterLayer &destination) const noexcept
{
    if (empty()) return true;
    if (!containsPatch(destination, m_bounds)) return false;

    std::size_t word = 0;
    if (!m_runLength) {
        for (int y = 0; y < m_bounds.height; ++y) {
            auto *row = destination.pixels.data() + rowStart(destination, m_bounds, y);
            for (int x = 0; x < m_bounds.width; ++x) row[x] ^= m_words[word++];
        }
        return true;
    }

    // Split a run only when it crosses the end of a destination row. Zero runs require no stores.
    std::size_t remaining = 0;
    std::uint32_t value = 0;
    for (int y = 0; y < m_bounds.height; ++y) {
        auto *row = destination.pixels.data() + rowStart(destination, m_bounds, y);
        for (std::size_t x = 0; x < static_cast<std::size_t>(m_bounds.width);) {
            if (remaining == 0) {
                remaining = m_words[word++];
                value = m_words[word++];
            }
            const auto count = std::min(remaining, static_cast<std::size_t>(m_bounds.width) - x);
            if (value != 0) {
                for (std::size_t i = x; i < x + count; ++i) row[i] ^= value;
            }
            remaining -= count;
            x += count;
        }
    }
    return true;
}
