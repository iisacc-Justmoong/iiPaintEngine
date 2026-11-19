#pragma once

#include <Stroke/StrokePoint.h>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace ii::brush {
// Owner-executor state machine: all methods and application of completion
// decisions are serialized on the same owner. A Work lease keeps its read-only
// samples valid on a worker across input growth, cancellation and queue destruction.
class PreviewQueue final {
    using Samples = std::vector<StrokePoint>;
public:
    enum class Error { InvalidState, InputLimit, OutOfMemory, EpochExhausted };
    enum class Finish { NoChange, Pending, CommitCurrent };
    enum class Completion { Invalid, Discard, Publish, Commit, Abort, Advance };
    struct Ticket {
        std::uint64_t epoch = 0;
        std::uint64_t version = 0;
        bool final = false;
        friend bool operator==(Ticket, Ticket) = default;
    };
    struct Work {
        Ticket ticket;

        // The worker continues from this completed version of the same epoch.
        [[nodiscard]] std::uint64_t baseVersion() const noexcept { return baseVersion_; }
        // Views are valid while a copy of the Work lease is retained.
        [[nodiscard]] std::span<const StrokePoint> points() const noexcept;
        // Whole-stroke authoring can borrow the complete input on final work only.
        [[nodiscard]] std::span<const StrokePoint> finalPoints() const noexcept;

    private:
        friend class PreviewQueue;
        Work(Ticket ticket, std::uint64_t baseVersion, std::shared_ptr<const Samples> storage) noexcept;
        std::shared_ptr<const Samples> storage_;
        std::span<const StrokePoint> prefix_;
        std::uint64_t baseVersion_;
    };

    explicit PreviewQueue(std::size_t maxSamples = 65536) noexcept;
    PreviewQueue(const PreviewQueue &) = delete;
    PreviewQueue &operator=(const PreviewQueue &) = delete;
    [[nodiscard]] std::expected<void, Error> begin() noexcept;
    // The queue preserves samples exactly. The caller/renderer is responsible
    // for validating sensor values and monotonically ordered timestamps.
    [[nodiscard]] std::expected<void, Error> push(const StrokePoint &point) noexcept;
    // At most one work item is outstanding, including work from a cancelled
    // session. A null optional means there is no work to schedule yet.
    [[nodiscard]] std::expected<std::optional<Work>, Error> take() noexcept;
    // Nonempty strokes schedule final work, even when its sample range is empty.
    [[nodiscard]] std::expected<Finish, Error> finish() noexcept;
    // A worker must complete even after cancellation, so its slot is released.
    // Advance keeps a successful superseded checkpoint without publishing it.
    // Failed superseded work leaves the checkpoint available for retry.
    [[nodiscard]] Completion complete(Ticket ticket, bool success) noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool inFlight() const noexcept;
    [[nodiscard]] bool needsWork() const noexcept;
    [[nodiscard]] std::size_t sampleCount() const noexcept;
    [[nodiscard]] std::size_t retainedBytes() const noexcept;

private:
    enum class State { Idle, Active, Finishing };
    std::size_t maxSamples_;
    std::shared_ptr<Samples> points_;
    std::optional<Ticket> inFlight_;
    std::uint64_t epoch_ = 0, version_ = 0, processed_ = 0;
    State state_ = State::Idle;
};
} // namespace ii::brush
