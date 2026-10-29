#pragma once

#include "Core/PaintRect.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string_view>

enum class RasterHistoryOverflow { Reject, EvictOldest };
struct RasterHistoryLimits {
    std::size_t maximumBytes = 64 * 1024 * 1024;
    std::size_t maximumEntries = 32;
    std::size_t maximumPatches = 65536;
    std::size_t maximumInputBytes = 1024 * 1024 * 1024;
    RasterHistoryOverflow overflow = RasterHistoryOverflow::Reject;
};
// Borrowed, row-major samples. Plane ids are opaque to this lower-level module.
// Equal-plane rectangles must not overlap. A sample occupies 1..16 bytes.
struct RasterHistoryPatch {
    std::string_view plane;
    DevicePixelRect bounds{};
    std::size_t sampleBytes = 4;
    std::span<const std::byte> data;
};
enum class RasterHistoryCode {
    None, InvalidArgument, BudgetExceeded, ReadRejected, WriteRejected,
    Conflict, NoUndo, NoRedo, Reentrant
};
struct RasterHistoryResult {
    RasterHistoryCode code = RasterHistoryCode::None;
    bool changed = false;
    // Exact admitted command cost; a lower bound when a budget scan stops early.
    std::size_t entryBytes = 0;
    std::size_t affectedSamples = 0;
    std::size_t evictedEntries = 0;
    std::size_t discardedRedoEntries = 0;
    [[nodiscard]] bool ok() const noexcept { return code == RasterHistoryCode::None; }
    [[nodiscard]] std::string_view message() const noexcept;
};
struct RasterHistoryStatistics {
    std::size_t undoEntries = 0;
    std::size_t redoEntries = 0;
    // Logical owned command storage, including sample bytes and command metadata.
    // Excludes fixed instance/allocator overhead and transient planning buffers.
    std::size_t retainedBytes = 0;
};
namespace iiPaintEngineDetail { struct RasterHistoryState; }

class RasterEditHistory final {
public:
    // A read fills a contiguous row of samples and must remain stable until the
    // writer is called. A writer applies the entire batch atomically: false or
    // exception must leave its source unchanged. Both callbacks are synchronous.
    using Reader = std::function<bool(std::string_view, DevicePixelPoint, unsigned,
                                     std::span<std::byte>)>;
    using Writer = std::function<bool(std::span<const RasterHistoryPatch>)>;
    explicit RasterEditHistory(RasterHistoryLimits limits = {});
    RasterEditHistory(const RasterEditHistory&) noexcept;
    RasterEditHistory& operator=(const RasterEditHistory&);

    RasterHistoryResult apply(std::span<const RasterHistoryPatch> patches,
                              const Reader& read, const Writer& write,
                              std::string_view label = {});
    RasterHistoryResult undo(const Reader& read, const Writer& write);
    RasterHistoryResult redo(const Reader& read, const Writer& write);
    RasterHistoryResult clear(); // Explicit history reset; never changes source pixels.
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;
    [[nodiscard]] std::string_view undoLabel() const noexcept;
    [[nodiscard]] std::string_view redoLabel() const noexcept;
    [[nodiscard]] RasterHistoryStatistics statistics() const noexcept;
    [[nodiscard]] const RasterHistoryLimits& limits() const noexcept { return m_limits; }
    [[nodiscard]] const RasterHistoryResult& lastResult() const noexcept { return m_last; }

private:
    RasterHistoryResult step(bool forward, const Reader&, const Writer&);
    RasterHistoryResult remember(RasterHistoryResult result) noexcept;
    RasterHistoryLimits m_limits;
    std::shared_ptr<const iiPaintEngineDetail::RasterHistoryState> m_state;
    RasterHistoryResult m_last;
    bool m_busy = false;
};
