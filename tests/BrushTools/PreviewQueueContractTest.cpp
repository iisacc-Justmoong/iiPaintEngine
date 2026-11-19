#include <iiBrushTools/Gesture.h>
#include <iiBrushTools/PreviewQueue.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    check(bool(result) && result->has_value(), "expected work lease");
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
    check(first.points().size() == 1 && equal(first.points().front(), point(0)), "worker lease mutated");
    check(q.complete(first.ticket, true) == PreviewQueue::Completion::Advance && q.needsWork(), "stale preview published");
    auto latest = take(q);
    check(latest.baseVersion() == 1 && latest.points().size() == 99 && latest.ticket.final,
          "coalescing repeated or dropped samples");
    for (int i = 1; i < 100; ++i) check(equal(latest.points()[std::size_t(i-1)], point(i)), "sensor data changed");
    check(latest.finalPoints().size() == 100, "whole-stroke authoring lost the input prefix");
    check(q.complete(latest.ticket, true) == PreviewQueue::Completion::Commit && !q.active(), "latest final not committed");
    check(q.complete(latest.ticket, true) == PreviewQueue::Completion::Invalid, "completion replay accepted");
}
void published_finish()
{
    PreviewQueue q; check(bool(q.begin()) && bool(q.push(point(3))), "begin preview");
    auto work = take(q);
    check(q.complete(work.ticket, true) == PreviewQueue::Completion::Publish, "latest preview not published");
    const auto finish = q.finish();
    check(bool(finish) && *finish == PreviewQueue::Finish::Pending && q.needsWork(), "finalization work was lost");
    auto final = take(q);
    check(final.ticket.final && final.points().empty() && final.baseVersion() == work.ticket.version,
          "finalization must proceed from the completed checkpoint");
    check(q.complete(work.ticket, true) == PreviewQueue::Completion::Invalid && q.inFlight(),
          "non-final ticket matched a final lease for the same version");
    check(q.complete(final.ticket, true) == PreviewQueue::Completion::Commit && !q.active(), "final result not committed");
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
    check(q.finish() == PreviewQueue::Finish::Pending, "new finish");
    auto fresh = take(q); check(fresh.points().size() == 1 && equal(fresh.points()[0], point(8)), "new samples contaminated");
    check(fresh.ticket.epoch != old.ticket.epoch && equal(old.points().front(), point(1)), "old work reused cancelled storage");
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
    check(latest.baseVersion() == 0 && latest.points().size() == 2, "failed work incorrectly advanced the checkpoint");
    check(q.complete(latest.ticket, false) == PreviewQueue::Completion::Abort && !q.active(), "current failure not reported");
}

// An old lease survives buffer growth, cancellation, a later epoch and destruction of its owner.
void lease_lifetime()
{
    std::optional<PreviewQueue::Work> leased;
    {
        PreviewQueue q(128);
        check(bool(q.begin()) && bool(q.push(point(1))), "lease begin");
        leased.emplace(take(q));
        for (int i = 2; i <= 128; ++i) check(bool(q.push(point(i))), "lease append");
        q.cancel();
        check(bool(q.begin()) && bool(q.push(point(512))), "lease restart");
        check(q.complete(leased->ticket, true) == PreviewQueue::Completion::Discard, "old worker used new epoch");
    }
    check(leased->points().size() == 1 && equal(leased->points().front(), point(1)), "lease outlived its samples");
}

// Exercise worker output against a nonuniform immutable source in negative world coordinates.
std::uint32_t sourceColor(DevicePixelPoint point)
{
    return 0x80400000U | ((static_cast<std::uint32_t>(point.x) * 13U) & 255U) << 8U
                       | ((static_cast<std::uint32_t>(point.y) * 9U) & 255U);
}
struct CountedSource {
    mutable std::size_t reads = 0;
    mutable std::size_t rows = 0;
    bool rejectSecondTile = false;
};
std::uint32_t countedSource(const void* context, DevicePixelPoint point)
{
    const auto& source = *static_cast<const CountedSource*>(context);
    if (source.rejectSecondTile && point.x >= 64) throw std::invalid_argument("source tile temporarily unavailable");
    ++source.reads;
    return sourceColor(point);
}

// 행 단위 원본 경로에서도 같은 위치에서 의도한 원본 읽기 실패를 발생시킨다.
void countedSourceRow(const void* context, DevicePixelPoint point, std::span<std::uint32_t> destination)
{
    ++static_cast<const CountedSource*>(context)->rows;
    for (auto& pixel : destination) {
        pixel = countedSource(context, point);
        ++point.x;
    }
}

// Apply a worker result to owner staging without visiting tiles outside that result.
void applyTiles(std::vector<std::uint32_t>& destination, DevicePixelRect clip,
                const ii::brush::RenderedStroke& stroke)
{
    for (const auto& tile : stroke.tiles) {
        for (int y = 0; y < tile.bounds.height; ++y) {
            const auto start = static_cast<std::size_t>(tile.bounds.origin.y - clip.origin.y + y) * clip.width
                    + tile.bounds.origin.x - clip.origin.x;
            std::copy_n(tile.pixels.pixels.data() + static_cast<std::size_t>(y) * tile.bounds.width,
                        tile.bounds.width, destination.data() + start);
        }
    }
}

// Compare all ARGB pixels and cumulative placement counts after coalescing, publication and finish.
void render_golden(std::size_t count, ii::brush::Tool tool, bool authoring)
{
    using namespace ii::brush;
    Controls controls;
    check(controls.set(Parameter::Size, 8) && controls.set(Parameter::Flow, 42)
          && controls.set(Parameter::Stabilization, 48) && controls.set(Parameter::PostCorrection, authoring ? 5 : 0)
          && controls.set(Parameter::Start, authoring ? 15 : 0) && controls.set(Parameter::End, authoring ? 35 : 0),
          "preview control setup failed");
    constexpr DevicePixelRect clip{{-32, -32}, 192, 128};
    CountedSource source;
    const RenderTarget target{clip, {&source, countedSource, clip.origin, clip.width, clip.height}, 0xD0DA6124U};
    const Limits limits{.maxInputPoints = count};
    auto brush = resolve(tool, controls, target.argb, target.seed, target.dynamics);
    check(bool(brush), "preview brush resolution failed");
    std::vector<StrokePoint> inputs;
    inputs.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto t = static_cast<double>(i) / static_cast<double>(count - 1);
        auto p = point(static_cast<int>(i));
        p.position = {-24 + t * 142, 27 + 24 * std::sin(t * 8)};
        p.pressure = .25 + .075 * static_cast<double>(i % 10);
        inputs.push_back(p);
    }
    const auto full = renderGestureWithResolvedBrush(inputs, *brush, controls, target, limits);
    check(bool(full) && !full->tiles.empty(), "whole-stroke golden render failed");
    const auto wholeReads = source.reads;
    source.reads = 0;
    std::vector<std::uint32_t> background(static_cast<std::size_t>(clip.width) * clip.height);
    for (int y = 0; y < clip.height; ++y) {
        for (int x = 0; x < clip.width; ++x) {
            background[static_cast<std::size_t>(y) * clip.width + x] = sourceColor({x, y});
        }
    }
    auto expected = background;
    applyTiles(expected, clip, *full);
    auto staged = background;

    PreviewQueue queue(count);
    GesturePreviewRenderer worker(*brush, controls, target, limits);
    check(bool(queue.begin()) && bool(queue.push(inputs[0])), "preview start failed");
    auto first = take(queue);
    check(bool(queue.push(inputs[1])), "coalesced sample failed");
    auto firstFrame = worker.render(first);
    check(bool(firstFrame) && !firstFrame->stroke.tiles.empty() && !firstFrame->replacesStroke, "first preview lost the dot");
    check(queue.complete(first.ticket, true) == PreviewQueue::Completion::Advance, "stale worker rendered to the UI");
    applyTiles(staged, clip, firstFrame->stroke);
    check(!worker.render(first), "a processed lease was applied twice");

    std::size_t delivered = first.points().size();
    for (std::size_t i = 2; i < count; ++i) {
        check(bool(queue.push(inputs[i])), "preview sample append failed");
        if (i % 97 != 0 && i + 1 != count) continue;
        auto work = take(queue);
        delivered += work.points().size();
        auto rendered = worker.render(work);
        check(bool(rendered) && !rendered->replacesStroke
              && queue.complete(work.ticket, true) == PreviewQueue::Completion::Publish, "live preview did not continue");
        applyTiles(staged, clip, rendered->stroke);
    }
    check(delivered == count, "live worker input repeated a prefix");
    check(queue.finish() == PreviewQueue::Finish::Pending, "preview finish failed");
    auto last = take(queue);
    auto final = worker.render(last);
    check(last.points().empty() && bool(final) && final->replacesStroke == authoring,
          "preview finalization did not use the correct checkpoint");
    if (final->replacesStroke) staged = background;
    applyTiles(staged, clip, final->stroke);
    check(queue.complete(last.ticket, true) == PreviewQueue::Completion::Commit, "preview did not commit");
    check(staged == expected && final->stroke.dabCount == full->dabCount && final->stroke.length == full->length,
          "incremental pixels, distance, or dabs diverged from the full render");
    if (!authoring) check(source.reads == wholeReads, "incremental worker reread old source tiles");

    check(bool(queue.begin()) && bool(queue.push(inputs.front())), "preview restart failed");
    auto cancelled = take(queue);
    check(bool(worker.render(cancelled)), "preview from a new epoch failed");
    queue.cancel();
    worker.cancel(cancelled.ticket.epoch);
    check(queue.complete(cancelled.ticket, true) == PreviewQueue::Completion::Discard && !worker.render(cancelled),
          "cancelled preview was applied");
}

// 같은 시각의 점을 서로 다른 작업으로 보낼 때도 시상수 0을 안정화 꺼짐으로 처리해야 한다.
void render_tiny_stabilization()
{
    using namespace ii::brush;
    Controls controls;
    check(controls.set(Parameter::Size, 8) && controls.set(Parameter::Start, 0)
          && controls.set(Parameter::End, 0) && controls.set(Parameter::PostCorrection, 0)
          && controls.set(Parameter::Stabilization, 1e-200), "tiny stabilization controls");
    constexpr DevicePixelRect clip{{}, 96, 64};
    const RenderTarget target{clip};
    const auto brush = resolve(Tool::Paint, controls);
    check(bool(brush), "tiny stabilization brush resolution");
    std::vector<StrokePoint> inputs(3);
    inputs[0].position = {16, 20};
    inputs[1].position = {38, 38};
    inputs[2].position = {68, 24};
    for (auto& input : inputs) {
        input.time = 0;
        input.pressure = 1;
    }

    auto actual = std::vector<std::uint32_t>(clip.width * clip.height);
    auto expected = actual;
    PreviewQueue queue(inputs.size());
    GesturePreviewRenderer worker(*brush, controls, target);
    check(bool(queue.begin()), "tiny stabilization begin");
    for (const auto& input : inputs) {
        check(bool(queue.push(input)), "tiny stabilization input");
        auto work = take(queue);
        const auto preview = worker.render(work);
        check(preview && std::isfinite(preview->stroke.length)
              && queue.complete(work.ticket, true) == PreviewQueue::Completion::Publish,
              "tiny stabilization rejected a valid preview batch");
        applyTiles(actual, clip, preview->stroke);
    }
    check(queue.finish() == PreviewQueue::Finish::Pending, "tiny stabilization finish");
    auto last = take(queue);
    const auto final = worker.render(last);
    check(final && !final->replacesStroke && queue.complete(last.ticket, true) == PreviewQueue::Completion::Commit,
          "tiny stabilization preview did not commit");
    applyTiles(actual, clip, final->stroke);

    check(controls.set(Parameter::Stabilization, 0), "disabled stabilization controls");
    const auto full = renderGesture(inputs, Tool::Paint, controls, target);
    check(full && !full->tiles.empty(), "tiny stabilization reference render");
    applyTiles(expected, clip, *full);
    check(actual == expected && final->stroke.length == full->length && final->stroke.dabCount == full->dabCount,
          "underflowed stabilization diverged from disabled stabilization");
}

// A source failure after modifying a cached tile must leave the earlier checkpoint retryable.
void render_rollback(bool useRows)
{
    using namespace ii::brush;
    Controls controls;
    check(controls.set(Parameter::Size, 6) && controls.set(Parameter::Flow, 27)
          && controls.set(Parameter::Start, 0) && controls.set(Parameter::End, 0)
          && controls.set(Parameter::Stabilization, 0) && controls.set(Parameter::PostCorrection, 0), "rollback controls");
    constexpr DevicePixelRect clip{{}, 128, 32};
    CountedSource source;
    const RenderTarget target{clip, {&source, countedSource, {}, clip.width, clip.height}};
    const auto brush = resolve(Tool::Paint, controls);
    check(bool(brush), "rollback brush");
    std::vector<StrokePoint> inputs;
    for (const auto x : {12, 30, 78, 90, 98}) {
        StrokePoint p;
        p.position = {double(x), 12}; p.time = double(inputs.size());
        inputs.push_back(p);
    }
    PreviewQueue queue;
    GesturePreviewRenderer worker(*brush, controls, target, {}, useRows ? countedSourceRow : nullptr);
    check(bool(queue.begin()) && bool(queue.push(inputs[0])), "rollback first input");
    auto first = take(queue);
    auto initial = worker.render(first);
    check(bool(initial) && queue.complete(first.ticket, true) == PreviewQueue::Completion::Publish, "rollback first render");
    check(source.rows == (useRows ? static_cast<std::size_t>(clip.height) : 0), "preview source row dispatch");
    for (std::size_t i = 1; i < 4; ++i) check(bool(queue.push(inputs[i])), "rollback batch input");
    auto failed = take(queue);
    check(bool(queue.push(inputs.back())), "rollback coalesced input");
    source.rejectSecondTile = true;
    check(!worker.render(failed) && queue.complete(failed.ticket, false) == PreviewQueue::Completion::Discard,
          "source failure incorrectly advanced a checkpoint");
    source.rejectSecondTile = false;
    check(queue.finish() == PreviewQueue::Finish::Pending, "rollback finish");
    auto retry = take(queue);
    auto final = worker.render(retry);
    check(retry.baseVersion() == 1 && bool(final) && queue.complete(retry.ticket, true) == PreviewQueue::Completion::Commit,
          "retry did not continue from the last successful input");
    const auto full = renderGestureWithResolvedBrush(inputs, *brush, controls, target);
    check(bool(full), "rollback golden render");
    auto expected = std::vector<std::uint32_t>(clip.width * clip.height);
    auto actual = expected;
    applyTiles(expected, clip, *full);
    applyTiles(actual, clip, initial->stroke);
    applyTiles(actual, clip, final->stroke);
    check(actual == expected, "failed batch leaked pixel edits into its retry");
}
}
int main()
{
    try {
        empty(); coalesced_finish(); published_finish(); cancel_restart(); failure_and_budget(); lease_lifetime();
        render_tiny_stabilization();
        render_golden(100'000, ii::brush::Tool::Paint, false);
        render_golden(275, ii::brush::Tool::Eraser, false);
        render_golden(275, ii::brush::Tool::Paint, true);
        render_rollback(false);
        render_rollback(true);
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "preview queue contracts passed\n";
}
