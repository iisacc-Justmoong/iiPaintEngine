#include "Gesture.h"
#include <Brush/BrushPreset.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ii::brush {
#ifdef IIPAINTENGINE_COUNT_TILE_LOOKUPS
void countGestureTileLookup() noexcept;
#endif
namespace {
// 입력과 보정 결과에 같은 좌표 유한성 검사를 적용한다.
bool isFinitePosition(DocumentPoint position)
{
    return std::isfinite(position.x) && std::isfinite(position.y);
}

bool validInput(const StrokePoint& point)
{
    return isFinitePosition(point.position) && std::abs(point.position.x)<=1e9
        && std::abs(point.position.y)<=1e9
        && std::isfinite(point.time) && std::abs(point.time)<=1e12
        && std::isfinite(point.pressure) && point.pressure>=0 && point.pressure<=1
        && std::isfinite(point.tiltX) && std::abs(point.tiltX)<=1
        && std::isfinite(point.tiltY) && std::abs(point.tiltY)<=1
        && std::isfinite(point.rotationRadians)
        && std::isfinite(point.tangentialPressure) && std::abs(point.tangentialPressure)<=1
        && std::isfinite(point.velocity) && point.velocity>=0
        && std::isfinite(point.arcLength) && point.arcLength>=0
        && std::ranges::all_of(point.custom,[](double v){return std::isfinite(v);});
}

// 전체 획과 증분 미리보기는 같은 시상수를 사용하며, 계산 결과가 0이면 보정을 건너뛴다.
double stabilizationTimeConstant(const Controls& controls)
{
    const double amount = controls.get(Parameter::Stabilization) / 100.;
    return .25 * amount * amount;
}

// 양의 시상수로 내부 점을 안정화하고, 비유한 좌표는 다음 단계로 보내지 않는다.
std::expected<DocumentPoint, Error> stabilizedPosition(DocumentPoint previous, DocumentPoint current, double dt, double tau)
{
    const auto weight = -std::expm1(-dt / tau);
    const DocumentPoint position{previous.x + (current.x - previous.x) * weight,
                                 previous.y + (current.y - previous.y) * weight};
    if (!isFinitePosition(position)) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "gesture stabilization produced non-finite coordinates"});
    }
    return position;
}
// 첫 측정과 보정 뒤 재측정이 같은 누적 거리 버퍼를 사용한다.
void measure(PreparedGesture& gesture)
{
    gesture.distances.resize(gesture.points.size());
    gesture.distances.front() = 0;
    for(std::size_t i=1;i<gesture.points.size();++i) {
        const auto a=gesture.points[i-1].position,b=gesture.points[i].position;
        gesture.distances[i]=gesture.distances[i-1]+std::hypot(b.x-a.x,b.y-a.y);
    }
    gesture.length=gesture.distances.back();
}
// 선분 길이와 그 길이에 대한 가중 중심을 함께 보관한다.
struct Moment { double mass = 0; DocumentPoint center{}; };

// 큰 누적 모멘트를 빼지 않고 두 구간의 중심을 상대 거리로 병합한다.
Moment merge(Moment a, Moment b)
{
    if (a.mass == 0) return b;
    if (b.mass == 0) return a;
    const auto mass = a.mass + b.mass;
    const auto weight = b.mass / mass;
    return {mass, {a.center.x + (b.center.x - a.center.x) * weight,
                   a.center.y + (b.center.y - a.center.y) * weight}};
}

// 점이 3개 이상인 획에서 내부 노드 n-2개만 전위 순서로 저장한다.
// 리프는 원래 선분에서 계산하며, 가중 중심의 병합 순서와 O(log n) 질의 비용을 보존한다.
class ArcAverage {
public:
    // 측정된 거리가 준비된 획에 대해 구간 인덱스를 만든다.
    explicit ArcAverage(const PreparedGesture& gesture)
        : gesture_(gesture), nodes_(gesture.points.size() - 2)
    {
        build(0, 0, gesture_.points.size() - 1);
    }

    // 호 길이 범위와 겹치는 구간의 가중 중심을 반환한다.
    Moment query(double low, double high) const
    {
        return query(0, 0, gesture_.points.size() - 1, low, high);
    }

private:
    // 한 선분에서 겹치는 구간의 길이와 정규화된 중심 위치를 결합한다.
    Moment segment(std::size_t begin, double mass, double t) const
    {
        const auto a = gesture_.points[begin].position;
        const auto b = gesture_.points[begin + 1].position;
        return {mass, {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}};
    }

    // 왼쪽에 k개 선분이 있으면 내부 노드가 k-1개다. 오른쪽은 node+k에 시작한다.
    Moment build(std::size_t node, std::size_t begin, std::size_t end)
    {
        if (end == begin + 1) {
            return segment(begin, gesture_.distances[end] - gesture_.distances[begin], .5);
        }
        const auto leftSegments = (end - begin) / 2;
        const auto middle = begin + leftSegments;
        const auto left = build(node + 1, begin, middle);
        const auto right = build(node + leftSegments, middle, end);
        return nodes_[node] = merge(left, right);
    }

    // 완전히 포함된 구간의 캐시를 쓰고, 경계가 걸치는 선분만 잘라 계산한다.
    Moment query(std::size_t node, std::size_t begin, std::size_t end, double low, double high) const
    {
        const auto start = gesture_.distances[begin];
        const auto finish = gesture_.distances[end];
        if (finish <= low || start >= high || finish <= start) return {};

        const bool covered = low <= start && high >= finish;
        if (end == begin + 1) {
            if (covered) return segment(begin, finish - start, .5);
            const auto left = std::max(start, low);
            const auto right = std::min(finish, high);
            const auto t = ((left - start) + (right - start)) / (2 * (finish - start));
            return segment(begin, right - left, t);
        }
        if (covered) return nodes_[node];

        const auto leftSegments = (end - begin) / 2;
        const auto middle = begin + leftSegments;
        const auto left = query(node + 1, begin, middle, low, high);
        const auto right = query(node + leftSegments, middle, end, low, high);
        return merge(left, right);
    }

    const PreparedGesture& gesture_;
    std::vector<Moment> nodes_;
};
DevicePixelRect intersection(DevicePixelRect a,DevicePixelRect b)
{
    const auto x=std::max<std::int64_t>(a.origin.x,b.origin.x),y=std::max<std::int64_t>(a.origin.y,b.origin.y);
    const auto right=std::min(static_cast<std::int64_t>(a.origin.x)+a.width,static_cast<std::int64_t>(b.origin.x)+b.width);
    const auto bottom=std::min(static_cast<std::int64_t>(a.origin.y)+a.height,static_cast<std::int64_t>(b.origin.y)+b.height);
    return {{static_cast<int>(x),static_cast<int>(y)},static_cast<int>(std::max<std::int64_t>(0,right-x)),static_cast<int>(std::max<std::int64_t>(0,bottom-y))};
}
DevicePixelRect united(DevicePixelRect a,DevicePixelRect b)
{
    if(a.width<=0 || a.height<=0) return b;
    const auto x=std::min(a.origin.x,b.origin.x),y=std::min(a.origin.y,b.origin.y);
    const auto right=std::max(a.origin.x+a.width,b.origin.x+b.width),bottom=std::max(a.origin.y+a.height,b.origin.y+b.height);
    return {{x,y},right-x,bottom-y};
}
int tileCoordinate(int value) { const auto q=value/64;return value%64<0?q-1:q; }
struct TileStorage { DevicePixelRect bounds;RasterLayer original;StrokeCompositeBuffer pending; };

using TileKey = std::pair<int, int>; // 행, 열

struct TileKeyHash {
    // 부호 있는 두 좌표를 모두 섞어 행이나 열이 같은 타일도 다른 버킷에 분산한다.
    std::size_t operator()(TileKey key) const noexcept
    {
        auto hash = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(key.first)) << 32)
                    | static_cast<std::uint32_t>(key.second);
        hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
        hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
        return static_cast<std::size_t>(hash ^ (hash >> 31));
    }
};

using Tiles = std::unordered_map<TileKey, TileStorage, TileKeyHash>;

// 실제 타일 인덱스 조회를 세는 진입점이다. 계측은 회귀 테스트 번역 단위에만 포함된다.
auto findTile(auto& tiles, TileKey key)
{
#ifdef IIPAINTENGINE_COUNT_TILE_LOOKUPS
    countGestureTileLookup();
#endif
    return tiles.find(key);
}

// 원본과 겹치는 부분만 행으로 복사한다. 초기화된 나머지 픽셀은 투명하다.
void copyTileSource(TileStorage& tile, const RasterSourceSampler& source, RenderTarget::CopySourceRow copySourceRow)
{
    if (!source.sampleArgb) return;
    const auto region = intersection(tile.bounds, {source.origin, source.width, source.height});
    if (region.width <= 0 || region.height <= 0) return;

    const auto localX = region.origin.x - source.origin.x;
    const auto localY = region.origin.y - source.origin.y;
    const auto offsetX = region.origin.x - tile.bounds.origin.x;
    const auto offsetY = region.origin.y - tile.bounds.origin.y;
    for (int y = 0; y < region.height; ++y) {
        const DevicePixelPoint start{localX, localY + y};
        auto* destination = tile.original.pixels.data() +
            static_cast<std::size_t>(offsetY + y) * tile.bounds.width + offsetX;
        if (copySourceRow) {
            copySourceRow(source.context, start, {destination, static_cast<std::size_t>(region.width)});
        } else {
            for (int x = 0; x < region.width; ++x) {
                destination[x] = source.sampleArgb(source.context, {start.x + x, start.y});
            }
        }
    }
}

// Count work across a whole stroke, including strokes delivered in multiple worker batches.
struct RasterProgress {
    RasterDabStream stream;
    std::size_t projected = 0, touched = 0, dabs = 0;
};

// Both callers use the same immutable source, compositing policy, and projection budgets.
std::optional<Error> validateRender(const ResolvedBrush& brush, const RenderTarget& target,
                                    RenderTarget::CopySourceRow copySourceRow)
{
    if (brush.state.rasterizer.blendMode != RasterBlendMode::SourceOver
            && !(brush.eraser && brush.state.rasterizer.blendMode == RasterBlendMode::DestinationOut)) {
        return Error{ErrorCode::InvalidControls, "only source-over and eraser brushes are supported"};
    }
    const auto& clip = target.clip;
    if (clip.width <= 0 || clip.height <= 0 || std::abs(static_cast<std::int64_t>(clip.origin.x)) > 1'000'000'000
            || std::abs(static_cast<std::int64_t>(clip.origin.y)) > 1'000'000'000
            || static_cast<std::int64_t>(clip.origin.x) + clip.width > 1'000'000'000
            || static_cast<std::int64_t>(clip.origin.y) + clip.height > 1'000'000'000) {
        return Error{ErrorCode::InvalidInput, "render clip is outside the supported coordinate range"};
    }
    if (target.source.sampleArgb && (target.source.width <= 0 || target.source.height <= 0)) {
        return Error{ErrorCode::InvalidInput, "a source sampler requires positive bounds"};
    }
    if (copySourceRow && !target.source.sampleArgb) {
        return Error{ErrorCode::InvalidInput, "a source row copier requires a pixel sampler"};
    }
    return std::nullopt;
}

// A batch forks only the tiles it modifies. Validation, sampler calls, and output allocation
// finish before its state can replace a worker checkpoint.
class StrokeRasterBatch {
public:
    StrokeRasterBatch(const ResolvedBrush& brush, const RenderTarget& target, const Limits& limits,
                      RenderTarget::CopySourceRow copySourceRow, RasterProgress progress = {},
                      const Tiles* previous = nullptr)
        : brush_(brush), target_(target), limits_(limits), previous_(previous), progress_(progress),
          sampler_(target.source), copySourceRow_(copySourceRow), current_(1)
    {
        if (!sampler_.sampleArgb) {
            sampler_ = {nullptr, [](const void*, DevicePixelPoint) { return std::uint32_t{0}; },
                        target.clip.origin, target.clip.width, target.clip.height};
        }
    }

    // 저장된 스트림을 이어서 배치하고 이 이벤트의 dab을 즉시 합성한다.
    void append(const StrokePoint& point, bool finish, double authoredLength)
    {
        auto dabs = generateDabs(point, finish);
        compositeDabs(dabs, authoredLength);
    }

    // 전체 보정 길이가 확정될 때까지 예산을 통과한 이벤트 버퍼를 보관한다.
    // 버퍼를 이동해 dab을 다시 생성하거나 복사하지 않으며, 합성한 버퍼부터 해제한다.
    void appendWithSmoothedTaper(std::span<const StrokePoint> points)
    {
        std::vector<std::vector<BrushDab>> buffered;
        for (std::size_t i = 0; i < points.size(); ++i) {
            auto dabs = generateDabs(points[i], i + 1 == points.size());
            if (!dabs.empty()) buffered.push_back(std::move(dabs));
        }
        const auto length = progress_.stream.traveledDistance;
        for (auto& event : buffered) {
            auto dabs = std::move(event);
            compositeDabs(dabs, length);
        }
    }

    // Return only tiles touched in this batch. Deltas may restore a tile to its original colors.
    RenderedStroke render(bool delta) const
    {
        RenderedStroke result;
        result.length = progress_.stream.traveledDistance;
        result.dabCount = progress_.dabs;
        result.tiles.reserve(edits_.size());
        for (const auto& [key, tile] : edits_) {
            auto pixels = tile.original;
            if (brush_.eraser) eraseStrokeBufferFromLayer(pixels, tile.pending);
            else compositeStrokeBufferOntoLayer(pixels, tile.pending);
            if (delta || pixels.pixels != tile.original.pixels) {
                result.dirty = united(result.dirty, tile.bounds);
                result.tiles.push_back({tile.bounds, std::move(pixels)});
            }
        }
        // 해시 인덱스의 순회 순서와 관계없이 공개 타일 목록의 행·열 순서를 지킨다.
        std::ranges::sort(result.tiles, {}, [](const PixelTile& tile) {
            return TileKey{tile.bounds.origin.y, tile.bounds.origin.x};
        });
        return result;
    }

    const RasterProgress& progress() const noexcept { return progress_; }

    // 체크포인트 용량을 확보한 뒤 이미 할당된 노드를 합친다. 용량 확보 실패는 이전 타일을 보존한다.
    void apply(std::optional<Tiles>& checkpoint, bool newEpoch)
    {
        if (!checkpoint || newEpoch) {
            if (!checkpoint) checkpoint.emplace();
            checkpoint->swap(edits_);
            return;
        }

        auto& tiles = *checkpoint;
        if (newTiles_ > tiles.max_size() - tiles.size()) throw std::length_error("gesture exceeds tile index capacity");
        const auto total = tiles.size() + newTiles_;
        // 기본 최대 로드 팩터 1에 맞추고, 새 타일이 조금씩 들어와도 기하급수적으로 늘린다.
        if (total > tiles.bucket_count()) {
            const auto grown = tiles.size() + std::min(tiles.size(), tiles.max_size() - tiles.size());
            tiles.reserve(std::max(total, grown));
        }
        for (const auto& [key, tile] : edits_) tiles.erase(key);
        tiles.merge(edits_);
    }

private:
    // 각 이벤트를 한 번 생성하고 전체 획에 누적된 dab 예산을 검사한다.
    std::vector<BrushDab> generateDabs(const StrokePoint& point, bool finish)
    {
        auto dabs = appendRasterDabs(progress_.stream, point, brush_.state, finish);
        if (dabs.size() > limits_.maxDabs - progress_.dabs) {
            throw std::length_error("gesture exceeds dab budget");
        }
        progress_.dabs += dabs.size();
        return dabs;
    }

    // 배치 순서대로 테이퍼 크기와 투영 예산을 적용해 타일의 누적 픽셀을 갱신한다.
    void compositeDabs(std::span<BrushDab> dabs, double authoredLength)
    {
        const auto& rasterizer = brush_.state.rasterizer;
        const auto& clip = target_.clip;
        for (auto& dab : dabs) {
            dab.scale *= taperSize(brush_.taper, dab.strokeDistance, authoredLength);
            if (dab.scale <= 0) continue;
            if (brush_.hairline) dab.ellipseScaleY = 1 / std::max(1e-9, rasterizer.brushSize * dab.scale);
            const auto bounds = deviceBoundsForBrushDab(dab, rasterizer, {});
            const auto clipped = intersection(bounds, clip);
            if (clipped.width <= 0 || clipped.height <= 0) continue;
            const auto area = static_cast<std::uint64_t>(bounds.width) * bounds.height;
            if (area > limits_.maxProjectedPixels - progress_.projected) {
                throw std::length_error("gesture exceeds projection budget");
            }
            progress_.projected += static_cast<std::size_t>(area);
            auto samples = projectBrushDabs(BrushDabSpan{&dab, 1}, rasterizer, {}, sampler_, brush_.state.material);
            for (auto sample : samples) {
                const auto world = sample.position;
                if (world.x < clip.origin.x || world.y < clip.origin.y
                        || world.x >= clip.origin.x + clip.width || world.y >= clip.origin.y + clip.height) continue;
                auto& tile = editTile(tileCoordinate(world.x), tileCoordinate(world.y));
                sample.position = {world.x - tile.bounds.origin.x, world.y - tile.bounds.origin.y};
                sample.blendMode = RasterBlendMode::SourceOver;
                current_[0] = sample;
                accumulateStrokeSamples(tile.pending, current_);
            }
        }
    }

    // 연속된 샘플은 직전 타일의 주소를 재사용한다. 노드 주소는 해시 테이블 확장 중에도 유효하다.
    TileStorage& editTile(int column, int row)
    {
        const TileKey key{row, column};
        if (lastTile_ && key == lastTileKey_) return *lastTile_;
        auto& tile = findOrEditTile(key);
        lastTileKey_ = key;
        lastTile_ = &tile;
        return tile;
    }

    // 새 타일의 원본은 한 번 수집하며, 기존 체크포인트 타일의 픽셀 커버리지를 이어간다.
    TileStorage& findOrEditTile(TileKey key)
    {
        if (const auto found = findTile(edits_, key); found != edits_.end()) return found->second;
        if (previous_) {
            if (const auto found = findTile(*previous_, key); found != previous_->end()) {
                return edits_.emplace(key, found->second).first->second;
            }
        }
        const auto [row, column] = key;
        const auto bounds = intersection({{column * 64, row * 64}, 64, 64}, target_.clip);
        const auto pixels = static_cast<std::size_t>(bounds.width) * bounds.height;
        if (pixels > limits_.maxTouchedPixels - progress_.touched) {
            throw std::length_error("gesture exceeds touched-pixel budget");
        }
        progress_.touched += pixels;
        TileStorage tile{bounds, makeRasterLayer(bounds.width, bounds.height),
                         makeStrokeCompositeBuffer(bounds.width, bounds.height)};
        copyTileSource(tile, target_.source, copySourceRow_);
        ++newTiles_;
        return edits_.emplace(key, std::move(tile)).first->second;
    }

    const ResolvedBrush& brush_;
    const RenderTarget& target_;
    const Limits& limits_;
    const Tiles* previous_;
    RasterProgress progress_;
    RasterSourceSampler sampler_;
    RenderTarget::CopySourceRow copySourceRow_;
    std::vector<RasterSample> current_;
    Tiles edits_;
    std::size_t newTiles_ = 0;
    TileKey lastTileKey_{};
    TileStorage* lastTile_ = nullptr;
};
}

std::expected<PreparedGesture, Error> prepareGesture(std::span<const StrokePoint> inputs,const Controls& controls,const Limits& limits)
{
    if(inputs.empty()) return std::unexpected(Error{ErrorCode::InvalidInput,"gesture has no samples"});
    if(inputs.size()>limits.maxInputPoints) return std::unexpected(Error{ErrorCode::BudgetExceeded,"gesture exceeds input budget"});
    for(std::size_t i=0;i<inputs.size();++i)
        if(!validInput(inputs[i]) || (i>0 && inputs[i].time<inputs[i-1].time))
            return std::unexpected(Error{ErrorCode::InvalidInput,"gesture samples must be finite and time must be monotonic"});
    PreparedGesture result;result.points.assign(inputs.begin(),inputs.end());
    const auto tau=stabilizationTimeConstant(controls);
    if(tau>0 && inputs.size()>2) {
        for(std::size_t i=1;i+1<inputs.size();++i) {
            const auto dt=inputs[i].time-inputs[i-1].time;
            const auto previous=result.points[i-1].position,current=inputs[i].position;
            auto position=stabilizedPosition(previous,current,dt,tau);
            if (!position) return std::unexpected(std::move(position.error()));
            result.points[i].position=*position;
        }
    }
    measure(result);
    const auto radius=controls.get(Parameter::PostCorrection);
    if(radius>0 && result.length>0 && result.points.size()>2) {
        const ArcAverage average(result);
        // 질의가 끝날 때까지 안정화된 좌표를 보존하고, 내부 점의 보정 좌표만 따로 준비한다.
        std::vector<DocumentPoint> corrected;
        corrected.reserve(result.points.size() - 2);
        for(std::size_t i=1;i+1<result.points.size();++i) {
            const auto original = result.points[i].position;
            const auto s=result.distances[i];
            const auto h=std::min({radius,s,result.length-s});
            const auto moment = h > 1e-9 ? average.query(s - h, s + h) : Moment{};
            auto position = original;
            if (moment.mass > 0) {
                position = moment.center;
                const auto dx = position.x - original.x, dy = position.y - original.y;
                const auto distance = std::hypot(dx, dy);
                if (distance > h) position = {original.x + dx * h / distance, original.y + dy * h / distance};
                if (!isFinitePosition(position)) {
                    return std::unexpected(Error{ErrorCode::InvalidInput, "gesture post-correction produced non-finite coordinates"});
                }
            }
            corrected.push_back(position);
        }
        for (std::size_t i = 0; i < corrected.size(); ++i) result.points[i + 1].position = corrected[i];
        measure(result);
    }
    return result;
}

std::expected<RenderedStroke, Error> renderGesture(std::span<const StrokePoint> inputs,Tool tool,const Controls& controls,
    const RenderTarget& target,const Limits& limits)
{
    return renderGesture(inputs, tool, controls, target, limits, nullptr);
}

std::expected<RenderedStroke, Error> renderGesture(std::span<const StrokePoint> inputs, Tool tool, const Controls& controls,
    const RenderTarget& target, const Limits& limits, RenderTarget::CopySourceRow copySourceRow)
{
    return renderGestureWithPreset(inputs, tool, controls, BrushPreset{}, target, limits, copySourceRow);
}

std::expected<RenderedStroke, Error> renderGestureWithPreset(std::span<const StrokePoint> inputs,
    Tool tool,const Controls& controls,const BrushPreset& base,const RenderTarget& target,const Limits& limits)
{
    return renderGestureWithPreset(inputs, tool, controls, base, target, limits, nullptr);
}

std::expected<RenderedStroke, Error> renderGestureWithPreset(std::span<const StrokePoint> inputs,
    Tool tool, const Controls& controls, const BrushPreset& base, const RenderTarget& target,
    const Limits& limits, RenderTarget::CopySourceRow copySourceRow)
{
    auto brush=resolveWithPreset(tool,controls,base,target.argb,target.seed,target.dynamics);
    if(!brush) return std::unexpected(brush.error());
    return renderGestureWithResolvedBrush(inputs, *brush, controls, target, limits, copySourceRow);
}

namespace {

// A future endpoint can rescale earlier dabs, or change the symmetric correction window.
bool hasLengthDependentTaper(const TaperProfile& taper)
{
    return taper.strength > 0 && taper.minimumSize < 1 && (taper.start > 0 || taper.end > 0);
}

} // namespace

std::expected<RenderedStroke, Error> renderGestureWithResolvedBrush(std::span<const StrokePoint> inputs,
    const ResolvedBrush& brush, const Controls& controls, const RenderTarget& target, const Limits& limits)
{
    return renderGestureWithResolvedBrush(inputs, brush, controls, target, limits, nullptr);
}

std::expected<RenderedStroke, Error> renderGestureWithResolvedBrush(std::span<const StrokePoint> inputs,
    const ResolvedBrush& brush, const Controls& controls, const RenderTarget& target, const Limits& limits,
    RenderTarget::CopySourceRow copySourceRow)
{
    if (auto error = validateRender(brush, target, copySourceRow)) return std::unexpected(std::move(*error));
    auto gesture = prepareGesture(inputs, controls, limits);
    if (!gesture) return std::unexpected(gesture.error());

    try {
        StrokeRasterBatch batch(brush, target, limits, copySourceRow);
        const auto& rasterizer = brush.state.rasterizer;
        // 스무딩과 길이 비례 테이퍼를 함께 쓸 때만 합성을 길이 확정까지 지연한다.
        if (rasterizer.smoothingEnabled && rasterizer.smoothing > 0 && hasLengthDependentTaper(brush.taper)) {
            batch.appendWithSmoothedTaper(gesture->points);
        } else {
            for (std::size_t i = 0; i < gesture->points.size(); ++i) {
                batch.append(gesture->points[i], i + 1 == gesture->points.size(), gesture->length);
            }
        }
        return batch.render(false);
    } catch (const std::length_error& error) {
        return std::unexpected(Error{ErrorCode::BudgetExceeded, error.what()});
    } catch (const std::invalid_argument& error) {
        return std::unexpected(Error{ErrorCode::InvalidInput, error.what()});
    } catch (const std::bad_alloc&) {
        return std::unexpected(Error{ErrorCode::BudgetExceeded, "gesture storage exhausted"});
    }
}

namespace {

// The first point can paint immediately. Holding one raw point preserves the authored
// endpoint exactly, including terminal dab placement and smoothing catch-up on release.
struct PreviewProgress {
    RasterProgress raster;
    std::optional<StrokePoint> tail;
    DocumentPoint lastPrepared{};
    double lastTime = 0;
};

} // namespace

struct GesturePreviewRenderer::State {
    State(ResolvedBrush brush, Controls controls, RenderTarget target, Limits limits,
          RenderTarget::CopySourceRow copySourceRow)
        : brush(std::move(brush)), controls(std::move(controls)), target(target), limits(limits), copySourceRow(copySourceRow),
          wholeStroke(this->controls.get(Parameter::PostCorrection) > 0
                      || hasLengthDependentTaper(this->brush.taper))
    {
    }

    ResolvedBrush brush;
    Controls controls;
    RenderTarget target;
    Limits limits;
    RenderTarget::CopySourceRow copySourceRow;
    bool wholeStroke;
    std::uint64_t epoch = 0, version = 0;
    bool finalized = false;
    PreviewProgress progress;
    std::optional<Tiles> tiles;
};

GesturePreviewRenderer::GesturePreviewRenderer(ResolvedBrush brush, Controls controls, RenderTarget target, Limits limits)
    : GesturePreviewRenderer(std::move(brush), std::move(controls), target, limits, nullptr)
{
}

GesturePreviewRenderer::GesturePreviewRenderer(ResolvedBrush brush, Controls controls, RenderTarget target, Limits limits,
                                               RenderTarget::CopySourceRow copySourceRow)
    : state_(std::make_unique<State>(std::move(brush), std::move(controls), target, limits, copySourceRow))
{
}

GesturePreviewRenderer::~GesturePreviewRenderer() = default;

void GesturePreviewRenderer::cancel(std::uint64_t epoch) noexcept
{
    auto& state = *state_;
    if (epoch < state.epoch) return;
    state.tiles.reset();
    state.progress = {};
    state.epoch = epoch;
    state.version = 0;
    state.finalized = true;
}

std::expected<GesturePreviewUpdate, Error> GesturePreviewRenderer::render(const PreviewQueue::Work& work)
{
    auto& state = *state_;
    const bool newEpoch = work.ticket.epoch > state.epoch;
    const auto base = newEpoch ? 0 : state.version;
    const auto points = work.points();
    if (work.ticket.epoch == 0 || work.ticket.epoch < state.epoch || (!newEpoch && state.finalized)
            || work.baseVersion() != base || work.ticket.version == 0 || work.ticket.version < base
            || work.ticket.version - base != points.size() || (points.empty() && !work.ticket.final)) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "preview work does not follow the current checkpoint"});
    }
    if (work.ticket.version > state.limits.maxInputPoints) {
        return std::unexpected(Error{ErrorCode::BudgetExceeded, "gesture exceeds input budget"});
    }

    try {
        if (auto error = validateRender(state.brush, state.target, state.copySourceRow)) {
            return std::unexpected(std::move(*error));
        }

        if (work.ticket.final && state.wholeStroke) {
            // The full input is borrowed from the final queue lease for this single exact authoring pass.
            auto rendered = renderGestureWithResolvedBrush(work.finalPoints(), state.brush, state.controls,
                                                            state.target, state.limits, state.copySourceRow);
            if (!rendered) return std::unexpected(std::move(rendered.error()));
            state.tiles.reset();
            state.progress = {};
            state.epoch = work.ticket.epoch;
            state.version = work.ticket.version;
            state.finalized = true;
            return GesturePreviewUpdate{std::move(*rendered), true};
        }

        auto progress = newEpoch ? PreviewProgress{} : state.progress;
        const auto* previous = newEpoch || !state.tiles ? nullptr : &*state.tiles;
        StrokeRasterBatch batch(state.brush, state.target, state.limits, state.copySourceRow, progress.raster, previous);
        const auto tau = stabilizationTimeConstant(state.controls);
        bool hasFirst = !newEpoch && state.version != 0;
        for (const auto& point : points) {
            const auto previousTime = progress.tail ? progress.tail->time : progress.lastTime;
            if (!validInput(point) || (hasFirst && point.time < previousTime)) {
                return std::unexpected(Error{ErrorCode::InvalidInput, "gesture samples must be finite and time must be monotonic"});
            }
            if (!hasFirst) {
                batch.append(point, false, 0.);
                progress.lastPrepared = point.position;
                progress.lastTime = point.time;
                hasFirst = true;
                continue;
            }
            if (progress.tail) {
                auto interior = *progress.tail;
                if (tau > 0) {
                    auto position = stabilizedPosition(progress.lastPrepared, interior.position,
                                                       interior.time - progress.lastTime, tau);
                    if (!position) return std::unexpected(std::move(position.error()));
                    interior.position = *position;
                }
                batch.append(interior, false, 0.);
                progress.lastPrepared = interior.position;
                progress.lastTime = interior.time;
            }
            progress.tail = point;
        }
        if (work.ticket.final && progress.tail) {
            batch.append(*progress.tail, true, 0.);
            progress.tail.reset();
        }

        auto rendered = batch.render(true);
        if (work.ticket.final) state.tiles.reset();
        else batch.apply(state.tiles, newEpoch);
        progress.raster = batch.progress();
        state.progress = work.ticket.final ? PreviewProgress{} : progress;
        state.epoch = work.ticket.epoch;
        state.version = work.ticket.version;
        state.finalized = work.ticket.final;
        return GesturePreviewUpdate{std::move(rendered), false};
    } catch (const std::length_error& error) {
        return std::unexpected(Error{ErrorCode::BudgetExceeded, error.what()});
    } catch (const std::invalid_argument& error) {
        return std::unexpected(Error{ErrorCode::InvalidInput, error.what()});
    } catch (const std::bad_alloc&) {
        return std::unexpected(Error{ErrorCode::BudgetExceeded, "preview OOM"});
    }
}

} // namespace ii::brush
