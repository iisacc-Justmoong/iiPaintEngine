#include "RasterEditHistory.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace iiPaintEngineDetail {
struct HistoryPlane { std::string id; std::size_t sampleBytes; };
struct HistoryRun {
    std::size_t plane = 0;
    DevicePixelRect bounds{};
    std::vector<std::byte> before, after;
};
struct HistoryEntry {
    std::string label;
    std::vector<HistoryPlane> planes;
    std::vector<HistoryRun> runs;
    std::size_t bytes = 0, samples = 0;
};
struct RasterHistoryState {
    std::vector<std::shared_ptr<const HistoryEntry>> undo, redo;
    std::size_t bytes = 0;
};
}

namespace {
using namespace iiPaintEngineDetail;
constexpr std::size_t ReadSamples = 64;
struct BusyScope { bool& flag; ~BusyScope() { flag = false; } };
struct PlaneView { std::string_view id; std::size_t sampleBytes; };
struct RunExtent { std::size_t patch, row, x, length; };
struct Plan {
    std::vector<PlaneView> planes;
    std::vector<std::size_t> patchPlanes;
    std::vector<RunExtent> runs;
    std::size_t bytes = sizeof(HistoryEntry) + sizeof(std::shared_ptr<const HistoryEntry>);
    std::size_t samples = 0;
};
bool add(std::size_t& value, std::size_t amount)
{
    if (amount > std::numeric_limits<std::size_t>::max() - value) return false;
    value += amount;
    return true;
}
bool readRow(const RasterEditHistory::Reader& read, std::string_view plane,
             DevicePixelPoint first, std::size_t sampleBytes, std::span<std::byte> output)
{
    try { return read(plane, first, static_cast<unsigned>(sampleBytes), output); }
    catch (const std::bad_alloc&) { throw; }
    catch (...) { return false; }
}
bool writeAll(const RasterEditHistory::Writer& write, std::span<const RasterHistoryPatch> patches)
{
    try { return write(patches); }
    catch (const std::bad_alloc&) { throw; }
    catch (...) { return false; }
}
RasterHistoryCode validate(std::span<const RasterHistoryPatch> patches,
                           const RasterHistoryLimits& limits, Plan& plan)
{
    if (patches.size() > limits.maximumPatches) return RasterHistoryCode::BudgetExceeded;
    struct Event { std::int64_t x, top, bottom; bool start; std::size_t patch; };
    std::map<std::string_view,std::size_t,std::less<>> planes;
    std::vector<std::vector<Event>> events;
    std::size_t totalBytes = 0;
    plan.patchPlanes.reserve(patches.size());
    for (std::size_t i = 0; i < patches.size(); ++i) {
        const auto& p = patches[i];
        const auto& b = p.bounds;
        const auto right = static_cast<std::int64_t>(b.origin.x) + b.width;
        const auto bottom = static_cast<std::int64_t>(b.origin.y) + b.height;
        if (b.width <= 0 || b.height <= 0 || p.sampleBytes == 0 || p.sampleBytes > 16
            || b.origin.x < -1000000000 || b.origin.y < -1000000000
            || right > 1000000000 || bottom > 1000000000)
            return RasterHistoryCode::InvalidArgument;
        const auto width = static_cast<std::size_t>(b.width), height = static_cast<std::size_t>(b.height);
        if (width > std::numeric_limits<std::size_t>::max() / height
            || width * height > std::numeric_limits<std::size_t>::max() / p.sampleBytes
            || width * height * p.sampleBytes != p.data.size()) return RasterHistoryCode::InvalidArgument;
        if (p.data.size() > limits.maximumInputBytes - totalBytes) return RasterHistoryCode::BudgetExceeded;
        totalBytes += p.data.size();
        auto [found, inserted] = planes.try_emplace(p.plane, plan.planes.size());
        if (inserted) {
            plan.planes.push_back({p.plane,p.sampleBytes});
            events.emplace_back();
            if (!add(plan.bytes,sizeof(HistoryPlane)) || !add(plan.bytes,p.plane.size()))
                return RasterHistoryCode::BudgetExceeded;
        } else if (plan.planes[found->second].sampleBytes != p.sampleBytes) {
            return RasterHistoryCode::InvalidArgument;
        }
        const auto plane = found->second;
        plan.patchPlanes.push_back(plane);
        events[plane].push_back({b.origin.x,b.origin.y,bottom,true,i});
        events[plane].push_back({right,b.origin.y,bottom,false,i});
    }
    for (auto& planeEvents : events) {
        std::ranges::sort(planeEvents,[](const Event& a,const Event& b) {
            return std::tie(a.x,a.start,a.patch) < std::tie(b.x,b.start,b.patch);
        });
        std::map<std::int64_t,std::pair<std::int64_t,std::size_t>> active;
        for (const auto& event : planeEvents) {
            if (!event.start) { active.erase(event.top); continue; }
            const auto next = active.lower_bound(event.top);
            if (next != active.end() && next->first < event.bottom) return RasterHistoryCode::InvalidArgument;
            if (next != active.begin() && std::prev(next)->second.first > event.top) return RasterHistoryCode::InvalidArgument;
            active.emplace_hint(next,event.top,std::pair{event.bottom,event.patch});
        }
    }
    return RasterHistoryCode::None;
}
RasterHistoryCode scan(std::span<const RasterHistoryPatch> patches, const RasterEditHistory::Reader& read,
                       const RasterHistoryLimits& limits, Plan& plan)
{
    std::array<std::byte,ReadSamples*16> buffer{};
    for (std::size_t index = 0; index < patches.size(); ++index) {
        const auto& p = patches[index];
        const auto width = static_cast<std::size_t>(p.bounds.width);
        for (std::size_t row = 0; row < static_cast<std::size_t>(p.bounds.height); ++row) {
            bool inRun = false;
            std::size_t start = 0;
            for (std::size_t x = 0; x < width;) {
                const auto count = std::min(ReadSamples,width-x);
                auto output = std::span(buffer).first(count*p.sampleBytes);
                if (!readRow(read,p.plane,{p.bounds.origin.x+static_cast<int>(x),p.bounds.origin.y+static_cast<int>(row)},p.sampleBytes,output))
                    return RasterHistoryCode::ReadRejected;
                for (std::size_t sample = 0; sample < count; ++sample) {
                    const auto offset = x + sample;
                    const auto* next = p.data.data()+(row*width+offset)*p.sampleBytes;
                    const bool changed = std::memcmp(buffer.data()+sample*p.sampleBytes,next,p.sampleBytes) != 0;
                    if (changed) {
                        if (!inRun) {
                            start = offset; inRun = true;
                            if (!add(plan.bytes,sizeof(HistoryRun))) return RasterHistoryCode::BudgetExceeded;
                        }
                        if (!add(plan.bytes,p.sampleBytes*2)) return RasterHistoryCode::BudgetExceeded;
                        ++plan.samples;
                        if (plan.bytes > limits.maximumBytes || limits.maximumEntries == 0)
                            return RasterHistoryCode::BudgetExceeded;
                    } else if (inRun) {
                        plan.runs.push_back({index,row,start,offset-start}); inRun = false;
                    }
                }
                x += count;
            }
            if (inRun) plan.runs.push_back({index,row,start,width-start});
        }
    }
    return RasterHistoryCode::None;
}
std::vector<RasterHistoryPatch> views(const HistoryEntry& entry, bool after)
{
    std::vector<RasterHistoryPatch> result;
    result.reserve(entry.runs.size());
    for (const auto& run : entry.runs) {
        const auto& plane = entry.planes[run.plane];
        result.push_back({plane.id,run.bounds,plane.sampleBytes,after?std::span(run.after):std::span(run.before)});
    }
    return result;
}
RasterHistoryCode match(const HistoryEntry& entry, bool after, const RasterEditHistory::Reader& read)
{
    std::array<std::byte,ReadSamples*16> buffer{};
    for (const auto& run : entry.runs) {
        const auto& plane = entry.planes[run.plane];
        const auto& expected = after?run.after:run.before;
        for (std::size_t x = 0; x < static_cast<std::size_t>(run.bounds.width);) {
            const auto count = std::min(ReadSamples,static_cast<std::size_t>(run.bounds.width)-x);
            auto bytes = std::span(buffer).first(count*plane.sampleBytes);
            if (!readRow(read,plane.id,{run.bounds.origin.x+static_cast<int>(x),run.bounds.origin.y},plane.sampleBytes,bytes))
                return RasterHistoryCode::ReadRejected;
            if (std::memcmp(bytes.data(),expected.data()+x*plane.sampleBytes,bytes.size()) != 0)
                return RasterHistoryCode::Conflict;
            x += count;
        }
    }
    return RasterHistoryCode::None;
}
}

std::string_view RasterHistoryResult::message() const noexcept
{
    switch (code) {
    case RasterHistoryCode::None: return {};
    case RasterHistoryCode::InvalidArgument: return "invalid or overlapping raster history patches";
    case RasterHistoryCode::BudgetExceeded: return "raster history budget would be exceeded";
    case RasterHistoryCode::ReadRejected: return "raster history source read was rejected";
    case RasterHistoryCode::WriteRejected: return "atomic raster history write was rejected";
    case RasterHistoryCode::Conflict: return "raster history pixels conflict with the current source";
    case RasterHistoryCode::NoUndo: return "no raster command is available to undo";
    case RasterHistoryCode::NoRedo: return "no raster command is available to redo";
    case RasterHistoryCode::Reentrant: return "nested raster history operations are not allowed";
    }
    return "unknown raster history result";
}
RasterEditHistory::RasterEditHistory(RasterHistoryLimits limits)
    : m_limits(limits),m_state(std::make_shared<iiPaintEngineDetail::RasterHistoryState>()) {}
RasterEditHistory::RasterEditHistory(const RasterEditHistory& other) noexcept
    : m_limits(other.m_limits),m_state(other.m_state),m_last(other.m_last) {}
RasterEditHistory& RasterEditHistory::operator=(const RasterEditHistory& other)
{
    if (m_busy) throw std::logic_error("cannot replace an active raster history");
    m_limits = other.m_limits; m_state = other.m_state; m_last = other.m_last;
    return *this;
}
RasterHistoryResult RasterEditHistory::remember(RasterHistoryResult result) noexcept
{ m_last = result; return result; }
RasterHistoryStatistics RasterEditHistory::statistics() const noexcept
{ return {m_state->undo.size(),m_state->redo.size(),m_state->bytes}; }
bool RasterEditHistory::canUndo() const noexcept { return !m_state->undo.empty(); }
bool RasterEditHistory::canRedo() const noexcept { return !m_state->redo.empty(); }
std::string_view RasterEditHistory::undoLabel() const noexcept
{ return canUndo()?std::string_view(m_state->undo.back()->label):std::string_view{}; }
std::string_view RasterEditHistory::redoLabel() const noexcept
{ return canRedo()?std::string_view(m_state->redo.back()->label):std::string_view{}; }
RasterHistoryResult RasterEditHistory::clear()
{
    if (m_busy) return remember({RasterHistoryCode::Reentrant});
    m_state = std::make_shared<RasterHistoryState>();
    return remember({});
}
RasterHistoryResult RasterEditHistory::apply(std::span<const RasterHistoryPatch> patches,
    const Reader& read, const Writer& write, std::string_view label)
{
    if (m_busy) return remember({RasterHistoryCode::Reentrant});
    m_busy = true; BusyScope guard{m_busy};
    if (patches.empty()) return remember({});
    if (!read || !write || (m_limits.overflow != RasterHistoryOverflow::Reject
                            && m_limits.overflow != RasterHistoryOverflow::EvictOldest))
        return remember({RasterHistoryCode::InvalidArgument});
    Plan plan;
    if (!add(plan.bytes,label.size())) return remember({RasterHistoryCode::BudgetExceeded});
    if (const auto code = validate(patches,m_limits,plan); code != RasterHistoryCode::None)
        return remember({code});
    if (const auto code = scan(patches,read,m_limits,plan); code != RasterHistoryCode::None)
        return remember({code,false,plan.bytes,plan.samples});
    if (plan.runs.empty()) return remember({});
    auto next = std::make_shared<RasterHistoryState>(*m_state);
    const auto discardedRedo = next->redo.size();
    for (const auto& entry : next->redo) next->bytes -= entry->bytes;
    next->redo.clear();
    std::size_t evicted = 0;
    while (next->undo.size() >= m_limits.maximumEntries
            || plan.bytes > m_limits.maximumBytes - next->bytes) {
        if (m_limits.overflow == RasterHistoryOverflow::Reject || next->undo.empty())
            return remember({RasterHistoryCode::BudgetExceeded,false,plan.bytes,plan.samples});
        next->bytes -= next->undo.front()->bytes;
        next->undo.erase(next->undo.begin()); ++evicted;
    }
    auto entry = std::make_shared<HistoryEntry>();
    entry->label = label; entry->bytes = plan.bytes; entry->samples = plan.samples;
    entry->planes.reserve(plan.planes.size());
    for (const auto& plane : plan.planes) entry->planes.push_back({std::string(plane.id),plane.sampleBytes});
    entry->runs.reserve(plan.runs.size());
    for (const auto& extent : plan.runs) {
        const auto& p = patches[extent.patch];
        const auto bytes = extent.length*p.sampleBytes;
        HistoryRun run{plan.patchPlanes[extent.patch],
            {{p.bounds.origin.x+static_cast<int>(extent.x),p.bounds.origin.y+static_cast<int>(extent.row)},
                static_cast<int>(extent.length),1},std::vector<std::byte>(bytes),std::vector<std::byte>(bytes)};
        if (!readRow(read,p.plane,run.bounds.origin,p.sampleBytes,run.before))
            return remember({RasterHistoryCode::ReadRejected});
        std::memcpy(run.after.data(),p.data.data()+(extent.row*static_cast<std::size_t>(p.bounds.width)+extent.x)*p.sampleBytes,bytes);
        entry->runs.push_back(std::move(run));
    }
    next->undo.reserve(next->undo.size()+1);
    next->undo.push_back(entry); next->bytes += entry->bytes;
    auto patchesToWrite = views(*entry,true);
    if (!writeAll(write,patchesToWrite)) return remember({RasterHistoryCode::WriteRejected});
    m_state = std::move(next); // All allocations precede a successful atomic write.
    return remember({RasterHistoryCode::None,true,entry->bytes,entry->samples,evicted,discardedRedo});
}
RasterHistoryResult RasterEditHistory::step(bool forward, const Reader& read, const Writer& write)
{
    if (m_busy) return remember({RasterHistoryCode::Reentrant});
    m_busy = true; BusyScope guard{m_busy};
    if (!read || !write) return remember({RasterHistoryCode::InvalidArgument});
    const auto& source = forward?m_state->redo:m_state->undo;
    if (source.empty()) return remember({forward?RasterHistoryCode::NoRedo:RasterHistoryCode::NoUndo});
    const auto entry = source.back();
    if (const auto code = match(*entry,!forward,read); code != RasterHistoryCode::None)
        return remember({code});
    auto next = std::make_shared<RasterHistoryState>(*m_state);
    auto& from = forward?next->redo:next->undo;
    auto& to = forward?next->undo:next->redo;
    to.reserve(to.size()+1); to.push_back(entry); from.pop_back();
    auto patches = views(*entry,forward);
    if (!writeAll(write,patches)) return remember({RasterHistoryCode::WriteRejected});
    m_state = std::move(next);
    return remember({RasterHistoryCode::None,true,entry->bytes,entry->samples});
}
RasterHistoryResult RasterEditHistory::undo(const Reader& read,const Writer& write)
{ return step(false,read,write); }
RasterHistoryResult RasterEditHistory::redo(const Reader& read,const Writer& write)
{ return step(true,read,write); }
