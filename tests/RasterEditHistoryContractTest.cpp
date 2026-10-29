#include <iostream>
#include "RasterHistoryTestUtils.h"
#include <array>
#include <limits>
using namespace historytest;

namespace {
void groupAndNoOp()
{
    Planes p;p.add("paint",128,128);p.add("mask",128,128,8);
    RasterEditHistory history;
    const std::array<std::uint32_t,4> color{0xff123456U,0xffabcdefU,0xff654321U,0xff112233U};
    const std::array<double,2> mask{.25,.75};
    const std::array patches{patch<std::uint32_t>("paint",{{63,63},2,2},color),patch<double>("mask",{{1,2},2,1},mask)};
    auto result=history.apply(patches,p.reader(),p.writer(),"paint and mask");
    check(result.ok() && result.changed && p.writes==1,"multi-plane command is not one atomic writer call");
    check(p.at<std::uint32_t>("paint",64,64)==color[3] && p.at<double>("mask",2,2)==.75,"grouped patches were not applied");
    check(history.statistics().undoEntries==1 && history.statistics().retainedBytes<4096,"small edits retain complete planes");
    check(history.undo(p.reader(),p.writer()).ok(),"group undo failed");
    check(p.at<std::uint32_t>("paint",64,64)==0 && p.at<double>("mask",2,2)==0,"undo did not restore both planes");
    const auto before=history.statistics();
    const std::array<std::uint32_t,1> zero{0};
    const auto noop=patch<std::uint32_t>("paint",{{1,1},1,1},zero);
    result=history.apply(std::span(&noop,1),p.reader(),p.writer());
    check(result.ok() && !result.changed && history.statistics().redoEntries==before.redoEntries,"no-op cleared redo");
    check(history.redo(p.reader(),p.writer()).ok() && p.at<double>("mask",1,2)==.25,"group redo failed");
}
void validationAndFailure()
{
    Planes p;p.add("p",64,64);RasterEditHistory history;
    const std::array<std::uint32_t,4> data{1,2,3,4};
    const auto a=patch<std::uint32_t>("p",{{1,1},2,2},data);
    auto b=a;b.bounds.origin.x=2;
    const std::array overlap{a,b};
    auto r=history.apply(overlap,p.reader(),p.writer());
    check(!r.ok() && r.code==RasterHistoryCode::InvalidArgument && p.writes==0,"overlapping patches accepted");
    auto invalid=a;invalid.sampleBytes=0;
    check(!history.apply(std::span(&invalid,1),p.reader(),p.writer()).ok(),"zero sample size accepted");
    invalid=a;invalid.bounds.origin.x=std::numeric_limits<int>::max();
    check(!history.apply(std::span(&invalid,1),p.reader(),p.writer()).ok(),"overflowing rectangle accepted");
    p.rejectRead=true;
    check(history.apply(std::span(&a,1),p.reader(),p.writer()).code==RasterHistoryCode::ReadRejected,"read failure not reported");
    p.rejectRead=false;p.rejectWrite=true;
    r=history.apply(std::span(&a,1),p.reader(),p.writer());
    check(r.code==RasterHistoryCode::WriteRejected && history.statistics().undoEntries==0
        && p.at<std::uint32_t>("p",1,1)==0,"failed writer changed history/source");
    p.rejectWrite=false;check(history.apply(std::span(&a,1),p.reader(),p.writer()).ok(),"retry after rejected writer failed");
    const auto stats=history.statistics();p.rejectWrite=true;
    check(history.undo(p.reader(),p.writer()).code==RasterHistoryCode::WriteRejected
        && history.statistics().undoEntries==stats.undoEntries,"failed undo consumed history");
}
void budgetsAndBranch()
{
    Planes p;p.add("p",64,64);
    RasterHistoryLimits limits;limits.maximumEntries=2;limits.maximumBytes=65536;
    RasterEditHistory h(limits);
    auto apply=[&](int x,std::uint32_t value) {
        const auto a=patch<std::uint32_t>("p",{{x,0},1,1},std::span(&value,1));
        return h.apply(std::span(&a,1),p.reader(),p.writer());
    };
    check(apply(0,1).ok() && apply(1,2).ok(),"bounded history setup failed");
    const auto before=h.statistics();const auto writes=p.writes;
    check(apply(2,3).code==RasterHistoryCode::BudgetExceeded && p.writes==writes
        && h.statistics().retainedBytes==before.retainedBytes,"count budget rejection mutated source/history");
    check(h.undo(p.reader(),p.writer()).ok(),"branch setup undo failed");
    p.rejectWrite=true;
    check(!apply(3,4).ok() && h.statistics().redoEntries==1,"failed branch cleared redo");
    p.rejectWrite=false;auto branch=apply(3,4);
    check(branch.ok() && branch.discardedRedoEntries==1 && h.statistics().redoEntries==0,"successful branch did not explicitly discard redo");
    limits.overflow=RasterHistoryOverflow::EvictOldest;
    RasterEditHistory evict(limits);
    for(int x=0;x<3;++x) {
        const std::uint32_t value=100+x;
        const auto a=patch<std::uint32_t>("p",{{x,10},1,1},std::span(&value,1));
        const auto result=evict.apply(std::span(&a,1),p.reader(),p.writer());
        check(result.ok() && result.evictedEntries==static_cast<std::size_t>(x==2),"eviction was not reported");
    }
    check(evict.undo(p.reader(),p.writer()).ok() && evict.undo(p.reader(),p.writer()).ok()
        && !evict.canUndo() && p.at<std::uint32_t>("p",0,10)==100,"eviction removed the wrong history boundary");
    limits.maximumBytes=1;RasterEditHistory tiny(limits);
    const std::uint32_t value=99;const auto a=patch<std::uint32_t>("p",{{4,4},1,1},std::span(&value,1));
    check(tiny.apply(std::span(&a,1),p.reader(),p.writer()).code==RasterHistoryCode::BudgetExceeded
        && p.at<std::uint32_t>("p",4,4)==0,"byte budget is checked after mutation");
}
void conflictCopyAndReentrancy()
{
    Planes p;p.add("p",64,64);RasterEditHistory h;
    const std::uint32_t value=11;const auto a=patch<std::uint32_t>("p",{{2,2},1,1},std::span(&value,1));
    check(h.apply(std::span(&a,1),p.reader(),p.writer()).ok(),"conflict setup failed");
    auto copy=h;
    const std::uint32_t external=22;
    std::memcpy(p.values.at("p").data.data()+(2*64+2)*4,&external,4);
    check(h.undo(p.reader(),p.writer()).code==RasterHistoryCode::Conflict && h.canUndo(),"conflicting undo overwrote external pixels");
    std::memcpy(p.values.at("p").data.data()+(2*64+2)*4,&value,4);
    check(h.undo(p.reader(),p.writer()).ok() && copy.canUndo(),"copy-on-write histories share a mutable cursor");
    check(h.redo(p.reader(),p.writer()).ok(),"redo after conflict retry failed");
    const std::uint32_t next=33;const auto b=patch<std::uint32_t>("p",{{3,3},1,1},std::span(&next,1));
    const auto writer=[&](std::span<const RasterHistoryPatch> patches) {
        check(h.undo(p.reader(),p.writer()).code==RasterHistoryCode::Reentrant,"nested history edit was accepted");
        return p.write(patches);
    };
    check(h.apply(std::span(&b,1),p.reader(),writer).ok() && h.statistics().undoEntries==2,"reentrant guard damaged the outer command");
}
void rowBoundariesAndExceptions()
{
    Planes p;p.add("wide",256,2,8);RasterEditHistory h;
    std::array<std::uint64_t,130> data{};
    for(auto index:{0,63,64,65,128,129}) data[index]=100+index;
    const auto a=patch<std::uint64_t>("wide",{{0,1},130,1},data);
    const auto result=h.apply(std::span(&a,1),p.reader(),p.writer(),"row boundaries");
    check(result.ok() && result.affectedSamples==6 && h.undoLabel()=="row boundaries","changed runs split incorrectly at the read block boundary");
    check(p.at<std::uint64_t>("wide",64,1)==164 && p.at<std::uint64_t>("wide",129,1)==229,"row run payload is corrupt");
    check(h.undo(p.reader(),p.writer()).ok() && p.at<std::uint64_t>("wide",64,1)==0,"row boundary undo failed");
    const auto stats=h.statistics();
    const RasterEditHistory::Writer throwing=[](auto){throw std::runtime_error("atomic writer rejected");return false;};
    check(h.redo(p.reader(),throwing).code==RasterHistoryCode::WriteRejected
        && h.statistics().redoEntries==stats.redoEntries,"throwing writer consumed redo");
    const RasterEditHistory::Reader throwingRead=[](auto,auto,auto,auto){throw std::runtime_error("read rejected");return false;};
    check(h.redo(throwingRead,p.writer()).code==RasterHistoryCode::ReadRejected && h.canRedo(),"throwing reader consumed redo");
    check(h.redo(p.reader(),p.writer()).ok(),"retry after callback exceptions failed");
    const auto negativeReader=[&](std::string_view id,DevicePixelPoint first,unsigned bytes,std::span<std::byte> out) {
        first.x+=128;return p.read(id,first,bytes,out);
    };
    const auto negativeWriter=[&](std::span<const RasterHistoryPatch> patches) {
        std::vector<RasterHistoryPatch> translated(patches.begin(),patches.end());
        for(auto& patch:translated) patch.bounds.origin.x+=128;
        return p.write(translated);
    };
    const std::uint64_t value=77;const auto negative=patch<std::uint64_t>("wide",{{-64,0},1,1},std::span(&value,1));
    check(h.apply(std::span(&negative,1),negativeReader,negativeWriter).ok()
        && p.at<std::uint64_t>("wide",64,0)==77,"negative world coordinate patch failed");
    check(h.undo(negativeReader,negativeWriter).ok() && p.at<std::uint64_t>("wide",64,0)==0,"negative world coordinate undo failed");
}
}
int main()
{
    try {groupAndNoOp();validationAndFailure();budgetsAndBranch();conflictCopyAndReentrancy();rowBoundariesAndExceptions();
        std::cout<<"RASTER_HISTORY_OK atomic_planes=true sparse=true limits=true rollback=true conflicts=true cow=true\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
