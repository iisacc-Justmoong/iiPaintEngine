#include "PreviewQueue.h"

#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace ii::brush {
PreviewQueue::PreviewQueue(std::size_t maxSamples) noexcept : maxSamples_(maxSamples) {}

std::expected<void, PreviewQueue::Error> PreviewQueue::begin() noexcept
{
    if (state_ != State::Idle) return std::unexpected(Error::InvalidState);
    if (epoch_ == std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error::EpochExhausted);
    ++epoch_;
    points_.clear(); version_ = 0; lastStarted_ = 0; published_ = 0;
    state_ = State::Active;
    return {};
}

std::expected<void, PreviewQueue::Error> PreviewQueue::push(const StrokePoint &point) noexcept
{
    if (state_ != State::Active) return std::unexpected(Error::InvalidState);
    if (points_.size() >= maxSamples_ || points_.size() == points_.max_size()
            || version_ == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(Error::InputLimit);
    try { points_.push_back(point); }
    catch (const std::bad_alloc &) { return std::unexpected(Error::OutOfMemory); }
    catch (const std::length_error &) { return std::unexpected(Error::InputLimit); }
    ++version_;
    return {};
}

std::expected<std::optional<PreviewQueue::Work>, PreviewQueue::Error> PreviewQueue::take() noexcept
{
    if (!needsWork()) return std::optional<Work>{};
    try {
        auto snapshot = points_;
        const Ticket ticket{epoch_, version_};
        inFlight_ = ticket;
        lastStarted_ = version_;
        return std::optional<Work>{Work{ticket, std::move(snapshot)}};
    } catch (const std::bad_alloc &) { return std::unexpected(Error::OutOfMemory); }
      catch (const std::length_error &) { return std::unexpected(Error::InputLimit); }
}

std::expected<PreviewQueue::Finish, PreviewQueue::Error> PreviewQueue::finish() noexcept
{
    if (state_ != State::Active) return std::unexpected(Error::InvalidState);
    if (points_.empty()) {
        state_ = State::Idle;
        return Finish::NoChange;
    }
    if (!inFlight_ && published_ == version_) {
        state_ = State::Idle; points_.clear();
        return Finish::CommitCurrent;
    }
    state_ = State::Finishing;
    return Finish::Pending;
}

PreviewQueue::Completion PreviewQueue::complete(Ticket ticket, bool success) noexcept
{
    if (!inFlight_ || *inFlight_ != ticket) return Completion::Invalid;
    inFlight_.reset();
    if (state_ == State::Idle || ticket.epoch != epoch_ || ticket.version != version_)
        return Completion::Discard;
    if (!success) {
        state_ = State::Idle; points_.clear();
        return Completion::Abort;
    }
    if (state_ == State::Finishing) {
        state_ = State::Idle; points_.clear();
        return Completion::Commit;
    }
    published_ = version_;
    return Completion::Publish;
}

void PreviewQueue::cancel() noexcept
{
    state_ = State::Idle; points_.clear(); version_ = 0; lastStarted_ = 0; published_ = 0;
}
bool PreviewQueue::active() const noexcept { return state_ != State::Idle; }
bool PreviewQueue::inFlight() const noexcept { return inFlight_.has_value(); }
bool PreviewQueue::needsWork() const noexcept
{
    return active() && !inFlight_ && !points_.empty() && version_ != lastStarted_;
}
std::size_t PreviewQueue::sampleCount() const noexcept { return points_.size(); }
std::size_t PreviewQueue::retainedBytes() const noexcept { return points_.capacity() * sizeof(StrokePoint); }
} // namespace ii::brush
