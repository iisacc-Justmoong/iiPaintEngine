#include "FrozenSource.h"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <new>
#include <utility>
#include <vector>

namespace ii::brush {
namespace {
using Key=std::pair<std::int64_t,std::int64_t>; // row, column
struct Tile { std::array<std::uint32_t,64*64> pixels{}; };
using Tiles=std::map<Key,std::shared_ptr<const Tile>>;
constexpr std::size_t tileBytes=64U*64U*sizeof(std::uint32_t);
bool valid(DevicePixelRect r,bool allowEmpty=false) noexcept {
    if (r.width<0 || r.height<0 || (!allowEmpty && (!r.width || !r.height))) return false;
    const auto limit=std::int64_t(std::numeric_limits<int>::max())+1;
    return std::int64_t(r.origin.x)+r.width<=limit &&
           std::int64_t(r.origin.y)+r.height<=limit;
}
std::int64_t tileFor(std::int64_t x) noexcept {
    auto q=x/64; if (x%64<0) --q; return q;
}
}
struct FrozenSource::Data { DevicePixelRect bounds; Tiles tiles; };
struct FrozenSourceBuilder::Impl { DevicePixelRect bounds; std::size_t limit; Tiles tiles; bool busy=false; };
FrozenSourceBuilder::FrozenSourceBuilder(std::unique_ptr<Impl> p) noexcept : impl_(std::move(p)) {}
FrozenSourceBuilder::~FrozenSourceBuilder()=default;
FrozenSourceBuilder::FrozenSourceBuilder(FrozenSourceBuilder&&) noexcept=default;
FrozenSourceBuilder& FrozenSourceBuilder::operator=(FrozenSourceBuilder&&) noexcept=default;
std::expected<FrozenSourceBuilder,FrozenSourceError> FrozenSourceBuilder::create(
    DevicePixelRect bounds,std::size_t maximumTiles) {
    if (!valid(bounds) || maximumTiles>std::numeric_limits<std::size_t>::max()/tileBytes)
        return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::InvalidBounds});
    try { return FrozenSourceBuilder(std::make_unique<Impl>(Impl{bounds,maximumTiles,{}})); }
    catch (const std::bad_alloc&) { return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::Allocation}); }
}
std::expected<std::size_t,FrozenSourceError> FrozenSourceBuilder::add(DevicePixelRect region,const Read& read) {
    if (!impl_ || impl_->busy) return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::InvalidState});
    if (!valid(region,true)) return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::InvalidBounds});
    const auto b=impl_->bounds;
    const auto left=std::max<std::int64_t>(b.origin.x,region.origin.x);
    const auto top=std::max<std::int64_t>(b.origin.y,region.origin.y);
    const auto right=std::min(std::int64_t(b.origin.x)+b.width,std::int64_t(region.origin.x)+region.width);
    const auto bottom=std::min(std::int64_t(b.origin.y)+b.height,std::int64_t(region.origin.y)+region.height);
    if (right<=left || bottom<=top) return std::size_t{0};
    if (!read) return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::SourceUnavailable});
    impl_->busy=true;
    struct ResetBusy { Impl* impl; ~ResetBusy() { impl->busy=false; } } reset{impl_.get()};
    try {
        std::vector<Key> needed;
        for (auto row=tileFor(top);row<=tileFor(bottom-1);++row) {
            for (auto column=tileFor(left);column<=tileFor(right-1);++column) {
                const Key key{row,column};
                if (impl_->tiles.contains(key)) continue;
                if (needed.size()>=impl_->limit-impl_->tiles.size())
                    return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::BudgetExceeded});
                needed.push_back(key);
            }
        }
        Tiles staged;
        for (const auto [row,column] : needed) {
            auto tile=std::make_shared<Tile>();
            const auto x0=column*64,y0=row*64;
            const auto lx=std::max<std::int64_t>(x0,b.origin.x);
            const auto ly=std::max<std::int64_t>(y0,b.origin.y);
            const auto rx=std::min(x0+64,std::int64_t(b.origin.x)+b.width);
            const auto ry=std::min(y0+64,std::int64_t(b.origin.y)+b.height);
            for (auto y=ly;y<ry;++y) for (auto x=lx;x<rx;++x) {
                const auto color=read({int(x),int(y)});
                if (!color) return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::SourceUnavailable});
                tile->pixels[std::size_t(y-y0)*64+std::size_t(x-x0)]=*color;
            }
            staged.emplace(Key{row,column},std::move(tile));
        }
        const auto count=staged.size();
        impl_->tiles.merge(staged);
        return count;
    } catch (const std::bad_alloc&) {
        return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::Allocation});
    } catch (...) {
        return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::SourceUnavailable});
    }
}
std::expected<FrozenSource,FrozenSourceError> FrozenSourceBuilder::freeze() const {
    if (!impl_ || impl_->busy) return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::InvalidState});
    try { return FrozenSource(std::make_shared<FrozenSource::Data>(FrozenSource::Data{impl_->bounds,impl_->tiles})); }
    catch (const std::bad_alloc&) { return std::unexpected(FrozenSourceError{FrozenSourceErrorCode::Allocation}); }
}
std::size_t FrozenSourceBuilder::tileCount() const noexcept { return impl_?impl_->tiles.size():0; }
DevicePixelRect FrozenSource::bounds() const noexcept { return data_->bounds; }
std::size_t FrozenSource::tileCount() const noexcept { return data_->tiles.size(); }
std::size_t FrozenSource::logicalBytes() const noexcept { return data_->tiles.size()*tileBytes; }
std::optional<std::uint32_t> FrozenSource::sample(DevicePixelPoint p) const noexcept {
    const auto b=data_->bounds;
    if (p.x<b.origin.x || p.y<b.origin.y || std::int64_t(p.x)>=std::int64_t(b.origin.x)+b.width ||
        std::int64_t(p.y)>=std::int64_t(b.origin.y)+b.height) return std::uint32_t{0};
    const auto column=tileFor(p.x),row=tileFor(p.y);
    const auto it=data_->tiles.find({row,column});
    if (it==data_->tiles.end()) return std::nullopt;
    return it->second->pixels[std::size_t(std::int64_t(p.y)-row*64)*64+
                              std::size_t(std::int64_t(p.x)-column*64)];
}
RasterSourceSampler FrozenSourceSampler::sampler() noexcept {
    const auto b=source_.bounds();
    return {this,&FrozenSourceSampler::read,b.origin,b.width,b.height};
}
std::uint32_t FrozenSourceSampler::read(const void* context,DevicePixelPoint local) noexcept {
    const auto& self=*static_cast<const FrozenSourceSampler*>(context);
    const auto b=self.source_.bounds();
    if (local.x<0 || local.y<0 || local.x>=b.width || local.y>=b.height) return 0;
    const DevicePixelPoint p{int(std::int64_t(b.origin.x)+local.x),int(std::int64_t(b.origin.y)+local.y)};
    const auto pixel=self.source_.sample(p);
    if (!pixel) {
        self.missing_.store(true,std::memory_order_relaxed);
        try {
            const auto key=std::pair{tileFor(p.y),tileFor(p.x)};
            if (!self.missingTiles_.contains(key)) {
                if (self.missingTiles_.size()>=self.maxMissingTiles_)
                    self.reportIncomplete_.store(true,std::memory_order_relaxed);
                else self.missingTiles_.insert(key);
            }
        } catch (...) {
            self.reportAllocationFailure_.store(true,std::memory_order_relaxed);
        }
        return 0;
    }
    return *pixel;
}
std::vector<DevicePixelRect> FrozenSourceSampler::missingTiles() const {
    std::vector<DevicePixelRect> result;
    result.reserve(missingTiles_.size());
    for (const auto [row,column] : missingTiles_)
        result.push_back({{int(column*64),int(row*64)},64,64});
    return result;
}
} // namespace ii::brush
