#pragma once

#include <Stroke/StrokePoint.h>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace ii::brush {
// Owner-executor state machine: all methods and application of completion
// decisions must be serialized on the same owner. Work owns its input copy;
// rendering, worker lifetime and document/source snapshotting are external.
class PreviewQueue final {
public:
    enum class Error { InvalidState, InputLimit, OutOfMemory, EpochExhausted };
    enum class Finish { NoChange, Pending, CommitCurrent };
    enum class Completion { Invalid, Discard, Publish, Commit, Abort };
    struct Ticket {
        std::uint64_t epoch = 0;
        std::uint64_t version = 0;
        friend bool operator==(Ticket, Ticket) = default;
    };
    struct Work {
        Ticket ticket;
        std::vector<StrokePoint> points;
    };

    explicit PreviewQueue(std::size_t maxSamples = 65536) noexcept;
    [[nodiscard]] std::expected<void, Error> begin() noexcept;
    // The queue preserves samples exactly. The caller/renderer is responsible
    // for validating sensor values and monotonically ordered timestamps.
    [[nodiscard]] std::expected<void, Error> push(const StrokePoint &point) noexcept;
    // At most one work item is outstanding, including work from a cancelled
    // session. A null optional means there is no work to schedule yet.
    [[nodiscard]] std::expected<std::optional<Work>, Error> take() noexcept;
    [[nodiscard]] std::expected<Finish, Error> finish() noexcept;
    // A worker must complete even after cancellation, so its slot is released.
    // Invalid tickets have no side effects; failed latest work aborts the stroke.
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
    std::vector<StrokePoint> points_;
    std::optional<Ticket> inFlight_;
    std::uint64_t epoch_ = 0, version_ = 0, lastStarted_ = 0, published_ = 0;
    State state_ = State::Idle;
};
} // namespace ii::brush
