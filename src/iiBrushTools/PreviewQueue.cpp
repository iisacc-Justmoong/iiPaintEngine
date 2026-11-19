#include "PreviewQueue.h"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace ii::brush {
PreviewQueue::PreviewQueue(std::size_t maxSamples) noexcept : maxSamples_(maxSamples) {}

// Capture the span on the owner so reading Work never accesses the vector's mutable bookkeeping.
PreviewQueue::Work::Work(Ticket ticket, std::uint64_t baseVersion, std::shared_ptr<const Samples> storage) noexcept
    : ticket(ticket), storage_(std::move(storage)), prefix_(*storage_), baseVersion_(baseVersion)
{
}

std::span<const StrokePoint> PreviewQueue::Work::points() const noexcept
{
    if (!storage_) return {};
    return prefix_.subspan(static_cast<std::size_t>(baseVersion_));
}

std::span<const StrokePoint> PreviewQueue::Work::finalPoints() const noexcept
{
    return storage_ && ticket.final ? prefix_ : std::span<const StrokePoint>{};
}

std::expected<void, PreviewQueue::Error> PreviewQueue::begin() noexcept
{
    if (state_ != State::Idle) return std::unexpected(Error::InvalidState);
    if (epoch_ == std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error::EpochExhausted);
    ++epoch_;
    state_ = State::Active;
    return {};
}

std::expected<void, PreviewQueue::Error> PreviewQueue::push(const StrokePoint &point) noexcept
{
    if (state_ != State::Active) return std::unexpected(Error::InvalidState);
    const auto limit = std::min(maxSamples_, Samples{}.max_size());
    if (sampleCount() >= limit || version_ == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(Error::InputLimit);
    try {
        const auto capacity = points_ ? points_->capacity() : 0;
        if (sampleCount() == capacity) {
            const auto nextCapacity = capacity > limit / 2 ? limit : std::max<std::size_t>(1, capacity * 2);
            if (points_ && points_.use_count() == 1) {
                points_->reserve(nextCapacity);
            } else {
                // Grow a different buffer when a Work lease can still reference the existing elements.
                auto next = std::make_shared<Samples>();
                next->reserve(nextCapacity);
                if (points_) next->assign(points_->begin(), points_->end());
                points_ = std::move(next);
            }
        }
        // Within capacity the worker reads earlier elements and the owner constructs only the new element.
        points_->push_back(point);
    }
    catch (const std::bad_alloc &) { return std::unexpected(Error::OutOfMemory); }
    catch (const std::length_error &) { return std::unexpected(Error::InputLimit); }
    ++version_;
    return {};
}

std::expected<std::optional<PreviewQueue::Work>, PreviewQueue::Error> PreviewQueue::take() noexcept
{
    if (!needsWork()) return std::optional<Work>{};
    const Ticket ticket{epoch_, version_, state_ == State::Finishing};
    inFlight_ = ticket;
    return std::optional<Work>{Work{ticket, processed_, points_}};
}

std::expected<PreviewQueue::Finish, PreviewQueue::Error> PreviewQueue::finish() noexcept
{
    if (state_ != State::Active) return std::unexpected(Error::InvalidState);
    if (sampleCount() == 0) {
        cancel();
        return Finish::NoChange;
    }
    state_ = State::Finishing;
    return Finish::Pending;
}

PreviewQueue::Completion PreviewQueue::complete(Ticket ticket, bool success) noexcept
{
    if (!inFlight_ || *inFlight_ != ticket) return Completion::Invalid;
    inFlight_.reset();
    if (state_ == State::Idle || ticket.epoch != epoch_)
        return Completion::Discard;
    if (!success) {
        if (ticket.version != version_) return Completion::Discard;
        cancel();
        return Completion::Abort;
    }
    processed_ = ticket.version;
    if (ticket.final) {
        cancel();
        return Completion::Commit;
    }
    return ticket.version == version_ ? Completion::Publish : Completion::Advance;
}

void PreviewQueue::cancel() noexcept
{
    state_ = State::Idle;
    if (points_ && points_.use_count() == 1) points_->clear();
    else points_.reset();
    version_ = 0;
    processed_ = 0;
}
bool PreviewQueue::active() const noexcept { return state_ != State::Idle; }
bool PreviewQueue::inFlight() const noexcept { return inFlight_.has_value(); }
bool PreviewQueue::needsWork() const noexcept
{
    return active() && !inFlight_ && version_ != 0 && (processed_ != version_ || state_ == State::Finishing);
}
std::size_t PreviewQueue::sampleCount() const noexcept { return points_ ? points_->size() : 0; }
std::size_t PreviewQueue::retainedBytes() const noexcept { return points_ ? points_->capacity() * sizeof(StrokePoint) : 0; }
} // namespace ii::brush
