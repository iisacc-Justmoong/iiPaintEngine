#pragma once

#include <Stroke/Rasterizer.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace ii::brush {
inline constexpr int FrozenSourceTileSide=64;
enum class FrozenSourceErrorCode { InvalidBounds, InvalidState, BudgetExceeded, SourceUnavailable, Allocation };
struct FrozenSourceError { FrozenSourceErrorCode code; };
// An immutable source view. An uncovered in-bounds pixel returns nullopt;
// outside the source bounds is transparent. Copies share frozen tile storage.
class FrozenSource final {
public:
    [[nodiscard]] std::optional<std::uint32_t> sample(DevicePixelPoint) const noexcept;
    [[nodiscard]] DevicePixelRect bounds() const noexcept;
    [[nodiscard]] std::size_t tileCount() const noexcept;
    [[nodiscard]] std::size_t logicalBytes() const noexcept;
private:
    struct Data;
    explicit FrozenSource(std::shared_ptr<const Data> data) noexcept : data_(std::move(data)) {}
    std::shared_ptr<const Data> data_;
    friend class FrozenSourceBuilder;
};
// Owner-thread builder. The reader is called only while adding tiles and must
// return nullopt on unavailable data. A failed add publishes no partial tiles.
class FrozenSourceBuilder final {
public:
    using Read=std::function<std::optional<std::uint32_t>(DevicePixelPoint)>;
    [[nodiscard]] static std::expected<FrozenSourceBuilder,FrozenSourceError>
    create(DevicePixelRect bounds,std::size_t maximumTiles);
    ~FrozenSourceBuilder();
    FrozenSourceBuilder(FrozenSourceBuilder&&) noexcept;
    FrozenSourceBuilder& operator=(FrozenSourceBuilder&&) noexcept;
    FrozenSourceBuilder(const FrozenSourceBuilder&)=delete;
    FrozenSourceBuilder& operator=(const FrozenSourceBuilder&)=delete;
    [[nodiscard]] std::expected<std::size_t,FrozenSourceError> add(DevicePixelRect region,const Read&);
    [[nodiscard]] std::expected<FrozenSource,FrozenSourceError> freeze() const;
    [[nodiscard]] std::size_t tileCount() const noexcept;
private:
    struct Impl;
    explicit FrozenSourceBuilder(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};
// One reader per render job. A false complete() invalidates the entire render;
// missing source pixels must never be published as if they were transparent.
class FrozenSourceSampler final {
public:
    explicit FrozenSourceSampler(FrozenSource source,std::size_t maxMissingTiles=65536) noexcept
        : source_(std::move(source)),maxMissingTiles_(maxMissingTiles) {}
    FrozenSourceSampler(const FrozenSourceSampler&)=delete;
    FrozenSourceSampler& operator=(const FrozenSourceSampler&)=delete;
    [[nodiscard]] RasterSourceSampler sampler() noexcept;
    [[nodiscard]] bool complete() const noexcept { return !missing_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool reportComplete() const noexcept { return !reportIncomplete_.load(std::memory_order_relaxed) && !reportAllocationFailure_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool reportAllocationFailure() const noexcept { return reportAllocationFailure_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::vector<DevicePixelRect> missingTiles() const;
private:
    static std::uint32_t read(const void*,DevicePixelPoint) noexcept;
    FrozenSource source_;
    std::size_t maxMissingTiles_;
    mutable std::set<std::pair<std::int64_t,std::int64_t>> missingTiles_; // row, column
    mutable std::atomic<bool> missing_{false},reportIncomplete_{false},reportAllocationFailure_{false};
};
} // namespace ii::brush
