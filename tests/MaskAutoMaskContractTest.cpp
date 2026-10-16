#include "Application/MaskPaintSession.h"
#include <algorithm>
#include <iostream>

#define CHECK(c) do {if(!(c)){std::cerr<<__LINE__<<": " #c "\n";return 1;}}while(false)
namespace {
PointerEvent event(PointerEventPhase phase,double x,double y,double time=0)
{
    PointerEvent value;value.phase=phase;value.documentPosition={x,y};value.time=time;
    value.button=phase==PointerEventPhase::Press?PointerButton::Primary:PointerButton::None;
    value.primaryButtonDown=phase!=PointerEventPhase::Release;return value;
}
float at(const PixelMask& mask,int x,int y){return mask.coverage[static_cast<std::size_t>(y)*mask.width+x];}
}
int main()
{
    MaskPaintSession session;CHECK(session.createMask(12,8));
    std::vector<std::uint32_t> source(96,0xFFFF2020U);
    for(int y=0;y<8;++y)for(int x=6;x<12;++x)source[y*12+x]=0xFF2020FFU;
    source[2*12+4]=0x00FF2020U; // Hidden RGB must never enter the mask.
    source[3*12+4]=0x80FF2020U;
    CHECK(session.setColorGuidance(source));
    LocalMaskBrushOptions options;options.sizePixels=16;options.feather=0;options.autoMask=true;options.autoMaskTolerance=.15;
    CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,3,4)).accepted);
    CHECK(at(session.previewMask(),3,4)==1&&at(session.previewMask(),8,4)==0);
    CHECK(at(session.previewMask(),4,2)==0&&at(session.previewMask(),4,3)>0&&at(session.previewMask(),4,3)<1);
    CHECK(!session.setColorGuidance(source)&&session.strokeActive());
    CHECK(session.processPointerEvent(event(PointerEventPhase::Move,8,4,1)).accepted);
    CHECK(at(session.previewMask(),8,4)==0); // Stroke-start reference survives crossing the edge.
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,8,4,1)).committed);
    CHECK(at(session.mask(),8,4)==0);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,8,4,2)).accepted);
    CHECK(at(session.previewMask(),8,4)==1); // Next stroke samples its own start color.
    session.cancelStroke();
    const auto before=session.mask().coverage;
    CHECK(!session.processPointerEvent(event(PointerEventPhase::Press,4,2,3)).accepted);
    CHECK(session.mask().coverage==before&&!session.strokeActive());
    CHECK(!session.processPointerEvent(event(PointerEventPhase::Press,-1,4,3)).accepted);
    CHECK(!session.setColorGuidance(std::span<const std::uint32_t>(source).first(10)));
    CHECK(session.clearColorGuidance());
    CHECK(!session.processPointerEvent(event(PointerEventPhase::Press,3,4,3)).accepted);
    options.autoMask=false;CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,3,4,3)).accepted);
    CHECK(at(session.previewMask(),8,4)==1);session.cancelStroke();

    CHECK(session.createMask(6,4));source.assign(24,0xFF303030U);source[1*6+3]=0xFF343434U;source[1*6+4]=0xFF909090U;
    CHECK(session.setColorGuidance(source));source.assign(24,0xFFFFFFFFU); // Session owns its source pixel snapshot.
    options.autoMask=true;options.autoMaskTolerance=.1;options.sizePixels=10;CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,2,1,4)).accepted);
    CHECK(at(session.previewMask(),3,1)>0&&at(session.previewMask(),3,1)<at(session.previewMask(),2,1));
    CHECK(at(session.previewMask(),4,1)==0);session.cancelStroke();
    options.autoMaskTolerance=0;CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,2,1,5)).accepted);
    CHECK(at(session.previewMask(),3,1)==0&&at(session.previewMask(),2,1)==1);session.cancelStroke();
    options.mode=MaskBrushMode::Erase;CHECK(session.createMask(6,4,1)&&session.setColorGuidance(std::vector<std::uint32_t>(24,0xFF303030U)));
    CHECK(session.setBrushOptions(options));
    CHECK(session.processPointerEvent(event(PointerEventPhase::Press,2,1,6)).accepted&&at(session.previewMask(),2,1)==0);
    CHECK(session.processPointerEvent(event(PointerEventPhase::Release,2,1,6)).committed);
    CHECK(std::ranges::all_of(session.mask().coverage,[](float v){return v>=0&&v<=1;}));
    std::cout<<"Auto-mask color edges, frozen reference, alpha, lifetime and rejection passed\n";
}
