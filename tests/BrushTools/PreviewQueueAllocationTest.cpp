#include <iiBrushTools/PreviewQueue.h>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { thread_local bool fail = false, fired = false; }
void *operator new(std::size_t size)
{
    if (fail) { fired = true; throw std::bad_alloc(); }
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
    auto failedWork = queue.take();
    fail = false;
    if (!fired || failedWork || failedWork.error() != Q::Error::OutOfMemory
            || queue.sampleCount() != 1 || queue.inFlight() || !queue.needsWork()) {
        std::cerr << "failed snapshot lost samples or claimed a worker\n"; return 4;
    }
    auto work = queue.take();
    if (!work || !work->has_value() || work->value().points.size() != 1
            || work->value().points[0].pressure != a.pressure) return 5;
    auto ticket = work->value().ticket;
    if (!queue.finish() || queue.complete(ticket, true) != Q::Completion::Commit) return 6;
    std::cout << "preview allocation rollback passed\n";
}
