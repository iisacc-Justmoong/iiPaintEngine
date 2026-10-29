#include <iiBrushTools/FrozenSource.h>
#include <iiBrushTools/FrozenRender.h>
#include <Brush/BrushPreset.h>
#include <array>
#include <future>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
using namespace ii::brush;
namespace {
void check(bool ok,const char* what) {if(!ok) throw std::runtime_error(what);}
std::uint32_t color(DevicePixelPoint p) {
    return 0xff000000U | ((std::uint32_t(p.x)&255U)<<16) |
           ((std::uint32_t(p.y)&255U)<<8) | ((std::uint32_t(p.x)^std::uint32_t(p.y))&255U);
}
void sparse() {
    auto made=FrozenSourceBuilder::create({{-128,-128},8448,8448},3);
    check(bool(made),"create");
    auto builder=std::move(*made);
    std::size_t calls=0;
    FrozenSourceBuilder::Read read=[&](DevicePixelPoint p)->std::optional<std::uint32_t>{++calls;return color(p);};
    auto first=builder.add({{-128,-128},64,64},read);
    check(first && *first==1 && calls==4096,"negative tile capture");
    auto duplicate=builder.add({{-100,-100},1,1},read);
    check(duplicate && *duplicate==0 && calls==4096,"duplicate capture read again");
    auto frozen=builder.freeze(); check(bool(frozen),"freeze");
    check(frozen->sample({-100,-100})==color({-100,-100}) &&
          frozen->sample({-129,0})==0 && !frozen->sample({4096,4096}),
          "covered, outside and missing pixels were confused");
    FrozenSourceSampler missing(*frozen); auto sampler=missing.sampler();
    check(sampler.sampleArgb(sampler.context,{28,28})==color({-100,-100}) && missing.complete(),
          "sample coordinates");
    (void)sampler.sampleArgb(sampler.context,{4224,4224});
    check(!missing.complete(),"missing tile was silently accepted");
    auto far=builder.add({{8192,8192},64,64},read);
    check(far && *far==1,"far tile");
    auto newer=builder.freeze(); check(bool(newer) && newer->tileCount()==2 &&
        newer->logicalBytes()==2*64*64*4 && !frozen->sample({8192,8192}) &&
        newer->sample({8192,8192})==color({8192,8192}),
        "snapshots did not share immutable tile generations");
    auto other=builder.add({{0,0},64,64},read); check(other && *other==1,"third tile");
    const auto before=builder.tileCount();
    auto rejected=builder.add({{128,0},64,64},read);
    check(!rejected && rejected.error().code==FrozenSourceErrorCode::BudgetExceeded &&
          builder.tileCount()==before,"budget failure changed captured tiles");
    auto work=std::async(std::launch::async,[source=*newer] {
        FrozenSourceSampler worker(source); auto sampler=worker.sampler();
        const auto pixel=sampler.sampleArgb(sampler.context,{8320,8320});
        return std::pair{pixel,worker.complete()};
    });
    const auto result=work.get();
    check(result.first==color({8192,8192}) && result.second,"worker could not read frozen tiles");
}
std::uint32_t renderedPixel(const RenderedStroke& r,int x,int y) {
    for(const auto& tile:r.tiles) {
        const auto b=tile.bounds;
        if(x>=b.origin.x && y>=b.origin.y && x<b.origin.x+b.width && y<b.origin.y+b.height)
            return tile.pixels.pixels[std::size_t(y-b.origin.y)*b.width+x-b.origin.x];
    }
    return 0;
}
void frozen_render() {
    auto made=FrozenSourceBuilder::create({{0,0},128,64},2);
    check(bool(made),"render fixture");
    auto builder=std::move(*made);
    FrozenSourceBuilder::Read blue=[](DevicePixelPoint)->std::optional<std::uint32_t>{return 0xff0000ffU;};
    check(bool(builder.add({{0,0},64,64},blue)),"capture first source tile");
    auto partial=builder.freeze(); check(bool(partial),"freeze partial");
    Controls controls;
    for(auto p : {Parameter::Stabilization,Parameter::PostCorrection,Parameter::Pressure,Parameter::Tilt,
                  Parameter::Velocity,Parameter::Scatter,Parameter::Start,Parameter::End})
        check(controls.set(p,0),"render controls");
    check(controls.set(Parameter::Size,8) && controls.set(Parameter::Flow,100) &&
          controls.set(Parameter::Opacity,50) && controls.set(Parameter::Hardness,100),"render controls");
    StrokePoint point; point.position={96,32}; point.time=0;
    const std::array points{point};
    auto missing=renderFrozenGesture(points,Tool::Paint,controls,{{0,0},128,64},*partial,0xffff0000U);
    check(!missing && missing.error().code==FrozenRenderErrorCode::MissingSource &&
          missing.error().missingTiles.size()==1 &&
          missing.error().missingTiles[0].origin.x==64 &&
          missing.error().missingTiles[0].origin.y==0 &&
          missing.error().missingTiles[0].width==64 &&
          missing.error().missingTiles[0].height==64,
          "missing-source report did not identify the required tile");
    auto capped=renderFrozenGesture(points,Tool::Paint,controls,{{0,0},128,64},*partial,
                                    0xffff0000U,1,{}, {},0);
    check(!capped && capped.error().code==FrozenRenderErrorCode::MissingReportLimit,
          "incomplete missing-source report was not rejected");
    for(const auto tile:missing.error().missingTiles)
        check(bool(builder.add(tile,blue)),"capture reported source tile");
    auto complete=builder.freeze(); check(bool(complete),"freeze complete");
    auto rendered=renderFrozenGesture(points,Tool::Paint,controls,{{0,0},128,64},*complete,0xffff0000U);
    check(bool(rendered),"complete source render");
    const auto mixed=renderedPixel(*rendered,96,32);
    check((mixed>>24)==255 && ((mixed>>16)&255)>0 && (mixed&255)>0,
          "frozen render did not mix original source");

    auto emptyMade=FrozenSourceBuilder::create({{0,0},128,64},2);check(bool(emptyMade),"discovery setup");
    auto empty=std::move(*emptyMade);auto emptySnapshot=empty.freeze();check(bool(emptySnapshot),"empty snapshot");
    StrokePoint start=point; start.position={16,32}; start.time=0;
    StrokePoint end=point; end.position={110,32}; end.time=1;
    const std::array line{start,end};
    auto tooSmall=renderFrozenGesture(line,Tool::Paint,controls,{{0,0},128,64},*emptySnapshot,
                                      0xffff0000U,1,{}, {},1);
    check(!tooSmall && tooSmall.error().code==FrozenRenderErrorCode::MissingReportLimit,
          "two missing tiles were reported as a complete one-tile list");
    auto discovered=renderFrozenGesture(line,Tool::Paint,controls,{{0,0},128,64},*emptySnapshot,
                                       0xffff0000U,1,{}, {},2);
    check(!discovered && discovered.error().code==FrozenRenderErrorCode::MissingSource &&
          discovered.error().missingTiles.size()==2 &&
          discovered.error().missingTiles[0].origin.x==0 &&
          discovered.error().missingTiles[1].origin.x==64,
          "multi-tile discovery was incomplete or unordered");
    for(const auto tile:discovered.error().missingTiles)check(bool(empty.add(tile,blue)),"capture discovery");
    auto populated=empty.freeze();check(bool(populated),"populated snapshot");
    auto actual=renderFrozenGesture(line,Tool::Paint,controls,{{0,0},128,64},*populated,0xffff0000U);
    const RasterSourceSampler blueSampler{nullptr,[](const void*,DevicePixelPoint){return 0xff0000ffU;},
                                          {0,0},128,64};
    const RenderTarget target{{{0,0},128,64},blueSampler,0xffff0000U};
    auto expected=renderGesture(line,Tool::Paint,controls,target);
    check(actual && expected && actual->tiles.size()==expected->tiles.size(),"completed discovery render");
    for(std::size_t i=0;i<actual->tiles.size();++i) {
        const auto a=actual->tiles[i].bounds, e=expected->tiles[i].bounds;
        check(a.origin.x==e.origin.x && a.origin.y==e.origin.y && a.width==e.width && a.height==e.height &&
              actual->tiles[i].pixels.pixels==expected->tiles[i].pixels.pixels,
              "discovery output differs from fully available source");
    }
}
void frozen_preset() {
    auto made=FrozenSourceBuilder::create({{0,0},128,64},2);
    check(bool(made),"preset source setup");
    auto builder=std::move(*made);
    FrozenSourceBuilder::Read blue=[](DevicePixelPoint)->std::optional<std::uint32_t>{return 0xff0000ffU;};
    check(bool(builder.add({{0,0},64,64},blue)),"preset capture left tile");
    auto partial=builder.freeze();check(bool(partial),"preset partial source");
    Controls c;
    for(auto p:{Parameter::Stabilization,Parameter::PostCorrection,Parameter::Pressure,Parameter::Tilt,
                Parameter::Velocity,Parameter::Scatter,Parameter::Start,Parameter::End})
        check(c.set(p,0),"preset control");
    check(c.set(Parameter::Size,8) && c.set(Parameter::Opacity,100) && c.set(Parameter::Flow,100),"preset control");
    BrushPreset preset;
    preset.stroke.spacing=3;
    preset.stroke.symmetry.mode=BrushSymmetryMode::Vertical;
    preset.stroke.symmetry.center={64,32};
    preset.tip={2,2,std::vector<std::byte>(4,std::byte{255})};
    StrokePoint first;first.position={16,32};first.time=0;
    StrokePoint last;last.position={44,32};last.time=1;
    const std::array line{first,last};
    auto missing=renderFrozenGestureWithPreset(line,Tool::Paint,c,preset,{{0,0},128,64},*partial,0xffff0000U);
    check(!missing && missing.error().code==FrozenRenderErrorCode::MissingSource &&
          missing.error().missingTiles.size()==1 && missing.error().missingTiles[0].origin.x==64,
          "preset missing-source report failed");
    auto capped=renderFrozenGestureWithPreset(line,Tool::Paint,c,preset,{{0,0},128,64},*partial,
                                              0xffff0000U,1,{}, {},0);
    check(!capped && capped.error().code==FrozenRenderErrorCode::MissingReportLimit,
          "preset missing-source limit failed");
    check(bool(builder.add(missing.error().missingTiles.front(),blue)),"preset capture right tile");
    auto complete=builder.freeze();check(bool(complete),"preset complete source");
    auto actual=renderFrozenGestureWithPreset(line,Tool::Paint,c,preset,{{0,0},128,64},*complete,0xffff0000U);
    const RasterSourceSampler sampler{nullptr,[](const void*,DevicePixelPoint){return 0xff0000ffU;},{0,0},128,64};
    const RenderTarget target{{{0,0},128,64},sampler,0xffff0000U};
    auto expected=renderGestureWithPreset(line,Tool::Paint,c,preset,target);
    auto plain=renderFrozenGesture(line,Tool::Paint,c,{{0,0},128,64},*complete,0xffff0000U);
    check(actual && expected && plain && actual->tiles.size()==expected->tiles.size(),"preset full-source render failed");
    for(std::size_t i=0;i<actual->tiles.size();++i) {
        const auto a=actual->tiles[i].bounds,e=expected->tiles[i].bounds;
        check(a.origin.x==e.origin.x && a.origin.y==e.origin.y && a.width==e.width && a.height==e.height &&
              actual->tiles[i].pixels.pixels==expected->tiles[i].pixels.pixels,
              "frozen preset differs from the same fully available source");
    }
    check(renderedPixel(*actual,104,32)!=renderedPixel(*plain,104,32),
          "preset symmetry did not alter the reflected pixels");
    check(preset.stroke.spacing==3 && preset.stroke.symmetry.center.x==64 && preset.tip.mask.size()==4,
          "render modified the input preset");
    auto invalid=preset;
    invalid.stroke.symmetry.center.x=std::numeric_limits<double>::quiet_NaN();
    auto rejected=renderFrozenGestureWithPreset(line,Tool::Paint,c,invalid,{{0,0},128,64},*complete);
    check(!rejected && rejected.error().code==FrozenRenderErrorCode::BrushFailure && rejected.error().brush,
          "invalid preset did not preserve the authoring error");
}
void mixer_discovery() {
    auto made=FrozenSourceBuilder::create({{0,0},128,64},2);check(bool(made),"mixer setup");
    auto builder=std::move(*made);
    FrozenSourceBuilder::Read read=[](DevicePixelPoint p)->std::optional<std::uint32_t>{return color(p);};
    Controls c;
    for(auto p:{Parameter::Stabilization,Parameter::PostCorrection,Parameter::Pressure,Parameter::Tilt,
                Parameter::Velocity,Parameter::Scatter,Parameter::Start,Parameter::End})
        check(c.set(p,0),"mixer control");
    check(c.set(Parameter::Size,8) && c.set(Parameter::Hardness,100) &&
          c.set(Parameter::Opacity,100) && c.set(Parameter::Flow,100),"mixer control");
    StrokePoint start;start.position={16,32};start.time=0;
    StrokePoint end;end.position={110,32};end.time=1;
    const std::array line{start,end};
    auto source=builder.freeze();check(bool(source),"mixer freeze");
    std::optional<RenderedStroke> finished;
    int attempts=0;
    for(;attempts<4;++attempts) {
        auto result=renderFrozenGesture(line,Tool::Mixer,c,{{0,0},128,64},*source,0xffff0000U);
        if(result) {finished=std::move(*result);break;}
        check(result.error().code==FrozenRenderErrorCode::MissingSource &&
              !result.error().missingTiles.empty(),"mixer discovery failure");
        for(auto tile:result.error().missingTiles)check(bool(builder.add(tile,read)),"mixer tile capture");
        source=builder.freeze();check(bool(source),"mixer refreeze");
    }
    check(finished && attempts>=1,"mixer discovery did not converge");
    const RasterSourceSampler full{nullptr,[](const void*,DevicePixelPoint p){return color(p);},{0,0},128,64};
    const RenderTarget target{{{0,0},128,64},full,0xffff0000U};
    auto expected=renderGesture(line,Tool::Mixer,c,target);
    check(expected && finished->tiles.size()==expected->tiles.size(),"mixer full-source comparison");
    for(std::size_t i=0;i<finished->tiles.size();++i) {
        const auto a=finished->tiles[i].bounds,e=expected->tiles[i].bounds;
        check(a.origin.x==e.origin.x && a.origin.y==e.origin.y && a.width==e.width && a.height==e.height &&
              finished->tiles[i].pixels.pixels==expected->tiles[i].pixels.pixels,
              "mixer discovery differs from complete source");
    }
}
void sparse_8k_render() {
    constexpr int side=8192;
    auto made=FrozenSourceBuilder::create({{0,0},side,side},512);check(bool(made),"8K sparse setup");
    auto builder=std::move(*made);
    FrozenSourceBuilder::Read read=[](DevicePixelPoint p)->std::optional<std::uint32_t>{return color(p);};
    Controls c;
    for(auto p:{Parameter::Stabilization,Parameter::PostCorrection,Parameter::Pressure,Parameter::Tilt,
                Parameter::Velocity,Parameter::Scatter,Parameter::Start,Parameter::End})
        check(c.set(p,0),"8K control");
    check(c.set(Parameter::Size,4) && c.set(Parameter::Spacing,100) &&
          c.set(Parameter::Opacity,50) && c.set(Parameter::Flow,100),"8K control");
    StrokePoint first;first.position={16,16};first.time=0;
    StrokePoint last;last.position={8176,8176};last.time=10;
    const std::array line{first,last};
    auto empty=builder.freeze();check(bool(empty),"8K freeze");
    auto missing=renderFrozenGesture(line,Tool::Paint,c,{{0,0},side,side},*empty,0xffff0000U,
                                     1,{}, {},512);
    check(!missing && missing.error().code==FrozenRenderErrorCode::MissingSource &&
          missing.error().missingTiles.size()>100 && missing.error().missingTiles.size()<=512,
          "8K sparse discovery failed or exceeded report limit");
    for(auto tile:missing.error().missingTiles)check(bool(builder.add(tile,read)),"8K tile capture");
    check(builder.tileCount()==missing.error().missingTiles.size() &&
          builder.tileCount()*64U*64U*4U<16U*1024U*1024U,"8K capture was not sparse");
    auto captured=builder.freeze();check(bool(captured),"8K captured source");
    auto actual=renderFrozenGesture(line,Tool::Paint,c,{{0,0},side,side},*captured,0xffff0000U);
    const RasterSourceSampler full{nullptr,[](const void*,DevicePixelPoint p){return color(p);},{0,0},side,side};
    const RenderTarget target{{{0,0},side,side},full,0xffff0000U};
    auto expected=renderGesture(line,Tool::Paint,c,target);
    check(actual && expected && actual->tiles.size()==expected->tiles.size(),"8K full-source comparison");
    for(std::size_t i=0;i<actual->tiles.size();++i) {
        const auto a=actual->tiles[i].bounds,e=expected->tiles[i].bounds;
        check(a.origin.x==e.origin.x && a.origin.y==e.origin.y && a.width==e.width && a.height==e.height &&
              actual->tiles[i].pixels.pixels==expected->tiles[i].pixels.pixels,
              "8K sparse result differs from complete source");
    }
    std::cout<<"sparse_8k_tiles="<<builder.tileCount()<<" logical_bytes="<<builder.tileCount()*64U*64U*4U<<'\n';
}
void reentrancy() {
    auto made=FrozenSourceBuilder::create({{0,0},128,64},4); check(bool(made),"reentry setup");
    auto builder=std::move(*made);
    bool attempted=false, rejected=false, freezeRejected=false;
    FrozenSourceBuilder::Read pure=[](DevicePixelPoint p)->std::optional<std::uint32_t>{return color(p);};
    FrozenSourceBuilder::Read recursive=[&](DevicePixelPoint p)->std::optional<std::uint32_t>{
        if(!attempted) {
            attempted=true;
            auto nested=builder.add({{64,0},64,64},pure);
            auto nestedFreeze=builder.freeze();
            rejected=!nested && nested.error().code==FrozenSourceErrorCode::InvalidState;
            freezeRejected=!nestedFreeze && nestedFreeze.error().code==FrozenSourceErrorCode::InvalidState;
        }
        return color(p);
    };
    auto outer=builder.add({{0,0},64,64},recursive);
    check(outer && *outer==1 && attempted && rejected && freezeRejected && builder.tileCount()==1,
          "reentrant source capture changed the builder");
}
void exception_recovery() {
    auto made=FrozenSourceBuilder::create({{0,0},128,64},2); check(bool(made),"exception setup");
    auto builder=std::move(*made);
    FrozenSourceBuilder::Read throwing=[](DevicePixelPoint)->std::optional<std::uint32_t>{
        throw std::runtime_error("source failure");
    };
    auto failed=builder.add({{0,0},64,64},throwing);
    check(!failed && failed.error().code==FrozenSourceErrorCode::SourceUnavailable &&
          builder.tileCount()==0,"exception leaked a partial tile");
    FrozenSourceBuilder::Read valid=[](DevicePixelPoint p)->std::optional<std::uint32_t>{return color(p);};
    auto recovered=builder.add({{0,0},64,64},valid);
    check(recovered && *recovered==1 && builder.tileCount()==1,"exception left builder locked");
}
FrozenSource source_after_builder_destruction() {
    auto made=FrozenSourceBuilder::create({{-64,-64},128,128},1);
    check(bool(made),"lifetime setup");
    auto builder=std::move(*made);
    FrozenSourceBuilder::Read read=[](DevicePixelPoint p)->std::optional<std::uint32_t>{return color(p);};
    check(bool(builder.add({{-64,-64},64,64},read)),"lifetime capture");
    auto frozen=builder.freeze();check(bool(frozen),"lifetime freeze");
    return std::move(*frozen);
}
void independent_lifetime() {
    auto source=source_after_builder_destruction();
    auto task=std::async(std::launch::async,[source] {
        FrozenSourceSampler worker(source); auto sampler=worker.sampler();
        return std::pair{sampler.sampleArgb(sampler.context,{1,1}),worker.complete()};
    });
    const auto result=task.get();
    check(result.first==color({-63,-63}) && result.second,"snapshot did not outlive its builder");
}
void atomic_failure() {
    auto made=FrozenSourceBuilder::create({{0,0},256,128},8);
    check(bool(made),"failure fixture");
    auto builder=std::move(*made);
    std::size_t calls=0;
    FrozenSourceBuilder::Read failing=[&](DevicePixelPoint p)->std::optional<std::uint32_t>{
        ++calls; if(p.x>=64) return std::nullopt; return color(p);
    };
    auto rejected=builder.add({{0,0},128,64},failing);
    check(!rejected && rejected.error().code==FrozenSourceErrorCode::SourceUnavailable &&
          calls>4096 && builder.tileCount()==0,"source failure partially published tiles");
    auto invalid=FrozenSourceBuilder::create({{0,0},-1,10},8);
    check(!invalid && invalid.error().code==FrozenSourceErrorCode::InvalidBounds,"invalid bounds accepted");
}
}
int main() {try {sparse();atomic_failure();frozen_render();frozen_preset();mixer_discovery();sparse_8k_render();reentrancy();exception_recovery();independent_lifetime();std::cout<<"FROZEN_SOURCE_OK\n";}
            catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
