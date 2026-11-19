#include <iiBrushTools/PreviewQueue.h>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
thread_local bool fail = false, fired = false, measureTake = false;
thread_local std::size_t takeAllocatedBytes = 0;
}
void *operator new(std::size_t size)
{
    if (fail) { fired = true; throw std::bad_alloc(); }
    if (measureTake) takeAllocatedBytes += size;
    if (auto *p = std::malloc(std::max<std::size_t>(size, 1))) return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

int main()
{
    using Q = ii::brush::PreviewQueue;
    Q queue(8);
    if (!queue.begin()) return 1;
    StrokePoint a; a.position = {1, 2}; a.time = 1; a.pressure = .3;
    fail = true; fired = false;
    auto rejected = queue.push(a);
    fail = false;
    if (!fired || rejected || rejected.error() != Q::Error::OutOfMemory
            || queue.sampleCount() != 0 || queue.inFlight()) {
        std::cerr << "failed append changed the queued gesture\n"; return 2;
    }
    if (!queue.push(a)) return 3;
    fail = true; fired = false;
    const auto first = queue.take();
    fail = false;
    if (fired || !first || !first->has_value() || first->value().points().size() != 1
            || first->value().points().front().pressure != a.pressure) {
        std::cerr << "take allocated memory or lost input\n"; return 4;
    }
    // Growth may allocate on push. Failure must leave the leased point and queue intact.
    fail = true;
    fired = false;
    const auto failedGrowth = queue.push(a);
    fail = false;
    if (!fired || failedGrowth != std::unexpected(Q::Error::OutOfMemory) || queue.sampleCount() != 1
            || !queue.inFlight() || first->value().points().front().pressure != a.pressure) return 5;
    if (queue.complete(first->value().ticket, true) != Q::Completion::Publish) return 6;

    constexpr std::size_t sampleCount = 100'000;
    Q longStroke(sampleCount);
    if (!longStroke.begin()) return 7;
    std::size_t deliveredBytes = 0;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        a.custom[3] = static_cast<double>(i);
        if (!longStroke.push(a)) return 8;
        measureTake = true;
        const auto work = longStroke.take();
        measureTake = false;
        if (!work || !work->has_value() || work->value().baseVersion() != i
                || work->value().points().size() != 1 || work->value().points().front().custom[3] != a.custom[3]) return 9;
        deliveredBytes += work->value().points().size_bytes();
        if (longStroke.complete(work->value().ticket, true) != Q::Completion::Publish) return 10;
    }
    if (longStroke.finish() != Q::Finish::Pending) return 11;
    measureTake = true;
    const auto final = longStroke.take();
    measureTake = false;
    if (!final || !final->has_value() || !final->value().points().empty()
            || final->value().finalPoints().size() != sampleCount
            || longStroke.complete(final->value().ticket, true) != Q::Completion::Commit) return 12;
    if (takeAllocatedBytes != 0 || deliveredBytes != sampleCount * sizeof(StrokePoint)) {
        std::cerr << "take allocated " << takeAllocatedBytes << " bytes and delivered " << deliveredBytes << " bytes\n";
        return 13;
    }
    std::cout << sampleCount << " points, " << deliveredBytes << " leased sample bytes, "
              << takeAllocatedBytes << " bytes allocated by take\n";
}
