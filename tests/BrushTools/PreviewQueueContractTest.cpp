#include <iiBrushTools/PreviewQueue.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using ii::brush::PreviewQueue;
namespace {
void check(bool value, std::string_view message)
{
    if (!value) throw std::runtime_error(std::string(message));
}
StrokePoint point(int i)
{
    StrokePoint p;
    // Some stationary events have distinct timestamps and sensors.
    p.position = {double(i / 3), 5}; p.time = i * .002;
    p.pressure = (i % 10) / 10.0; p.tiltX = (i % 4) / 4.0;
    p.tiltY = -(i % 3) / 3.0; p.rotationRadians = i * .01;
    p.tangentialPressure = (i % 2) * .25; p.velocity = i * 2;
    p.arcLength = i * .5; p.deviceState = std::uint32_t(i % 7);
    p.custom[3] = i * .125;
    return p;
}
bool equal(const StrokePoint &a, const StrokePoint &b)
{
    return a.position.x == b.position.x && a.position.y == b.position.y
        && a.pressure == b.pressure && a.time == b.time && a.velocity == b.velocity
        && a.tiltX == b.tiltX && a.tiltY == b.tiltY && a.deviceState == b.deviceState
        && a.arcLength == b.arcLength && a.rotationRadians == b.rotationRadians
        && a.tangentialPressure == b.tangentialPressure && a.custom == b.custom;
}
PreviewQueue::Work take(PreviewQueue &q)
{
    auto result = q.take();
    check(bool(result) && result->has_value(), "expected work snapshot");
    return std::move(result->value());
}
void empty()
{
    PreviewQueue q(4);
    check(bool(q.begin()), "begin empty stroke");
    const auto end = q.finish();
    check(bool(end) && *end == PreviewQueue::Finish::NoChange && !q.active(), "empty finish");
    check(!q.finish(), "finish on inactive stroke accepted");
}
void coalesced_finish()
{
    PreviewQueue q(128); check(bool(q.begin()), "begin");
    check(bool(q.push(point(0))), "first sample");
    auto first = take(q);
    for (int i = 1; i < 100; ++i) check(bool(q.push(point(i))), "sample append");
    check(q.sampleCount() == 100 && q.inFlight() && !q.needsWork(), "multiple workers admitted");
    const auto finish = q.finish();
    check(bool(finish) && *finish == PreviewQueue::Finish::Pending, "finish must await latest input");
    check(!q.push(point(100)), "input accepted after finish");
    check(q.complete(first.ticket, true) == PreviewQueue::Completion::Discard && q.needsWork(), "stale preview published");
    auto latest = take(q);
    check(latest.points.size() == 100, "coalescing dropped samples");
    for (int i = 0; i < 100; ++i) check(equal(latest.points[std::size_t(i)], point(i)), "sensor data changed");
    check(q.complete(latest.ticket, true) == PreviewQueue::Completion::Commit && !q.active(), "latest final not committed");
    check(q.complete(latest.ticket, true) == PreviewQueue::Completion::Invalid, "completion replay accepted");
}
void published_finish()
{
    PreviewQueue q; check(bool(q.begin()) && bool(q.push(point(3))), "begin preview");
    auto work = take(q);
    check(q.complete(work.ticket, true) == PreviewQueue::Completion::Publish, "latest preview not published");
    const auto finish = q.finish();
    check(bool(finish) && *finish == PreviewQueue::Finish::CommitCurrent && !q.active(), "published latest not reused");
}
void cancel_restart()
{
    PreviewQueue q; check(bool(q.begin()) && bool(q.push(point(1))), "old session");
    auto old = take(q); q.cancel();
    check(bool(q.begin()) && bool(q.push(point(8))), "new session");
    auto noWork = q.take();
    check(bool(noWork) && !noWork->has_value(), "second worker started before old worker completed");
    auto wrong = old.ticket; ++wrong.version;
    check(q.complete(wrong, true) == PreviewQueue::Completion::Invalid && q.inFlight(), "wrong ticket released worker");
    check(q.complete(old.ticket, true) == PreviewQueue::Completion::Discard && q.needsWork(), "cancelled session published");
    auto fresh = take(q); check(fresh.points.size() == 1 && equal(fresh.points[0], point(8)), "new samples contaminated");
    check(q.finish() == PreviewQueue::Finish::Pending, "new finish");
    check(q.complete(fresh.ticket, true) == PreviewQueue::Completion::Commit, "new session did not commit");
}
void failure_and_budget()
{
    PreviewQueue q(2); check(bool(q.begin()), "budget begin");
    check(bool(q.push(point(0))) && bool(q.push(point(1))), "budget append");
    check(!q.push(point(2)) && q.sampleCount() == 2, "budget failure altered samples");
    auto first = take(q);
    check(q.complete(first.ticket, false) == PreviewQueue::Completion::Abort && !q.active(), "latest failure not aborted");
    check(bool(q.begin()) && bool(q.push(point(3))), "restart after failure");
    auto stale = take(q); check(bool(q.push(point(4))), "newer sample");
    check(q.complete(stale.ticket, false) == PreviewQueue::Completion::Discard && q.needsWork(), "stale error aborted latest");
    auto latest = take(q);
    check(q.complete(latest.ticket, false) == PreviewQueue::Completion::Abort && !q.active(), "current failure not reported");
}
}
int main()
{
    try { empty(); coalesced_finish(); published_finish(); cancel_restart(); failure_and_budget(); }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "preview queue contracts passed\n";
}
