#include "RasterHistoryTestUtils.h"
#include <array>
#include <chrono>
#include <iostream>

using namespace historytest;
namespace {
constexpr int Side=8192, PatchWidth=64, PatchHeight=16, Edits=48;
std::uint32_t mixed(std::uint32_t value)
{
    value^=value>>16;value*=0x7feb352dU;value^=value>>15;value*=0x846ca68bU;return value^(value>>16);
}
std::uint32_t color(std::size_t index,unsigned seed) { return mixed(static_cast<std::uint32_t>(index)+seed); }
double coverage(std::size_t index) { return mixed(static_cast<std::uint32_t>(index)+71)/4294967295.; }
template<class T> void put(Plane& plane,std::size_t index,T value)
{ std::memcpy(plane.data.data()+index*sizeof(T),&value,sizeof(T)); }
long long micros(std::chrono::steady_clock::time_point before)
{ return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-before).count(); }
}
int main()
{
    try {
        Planes p;p.add("paint-a",Side,Side);p.add("paint-b",Side,Side);p.add("mask",Side,Side,8);
        auto& a=p.values.at("paint-a");auto& b=p.values.at("paint-b");auto& mask=p.values.at("mask");
        const auto samples=static_cast<std::size_t>(Side)*Side;
        for(std::size_t i=0;i<samples;++i) {put(a,i,color(i,17));put(b,i,color(i,37));put(mask,i,coverage(i));}
        RasterHistoryLimits limits;limits.maximumEntries=128;limits.maximumBytes=4*1024*1024;
        RasterEditHistory history(limits);
        std::array<std::uint32_t,PatchWidth*PatchHeight> first{},second{};
        std::array<double,PatchWidth*PatchHeight> maskValues{};
        std::vector<long long> latencies;
        for(int edit=0;edit<Edits;++edit) {
            std::fill(first.begin(),first.end(),0xff000000U+static_cast<unsigned>(edit+1));
            std::fill(second.begin(),second.end(),0xff100000U+static_cast<unsigned>(edit+1));
            std::fill(maskValues.begin(),maskValues.end(),(edit+1)/64.);
            const DevicePixelRect bounds{{(edit*137)%(Side-PatchWidth),(edit*223)%(Side-PatchHeight)},PatchWidth,PatchHeight};
            const std::array batch{patch<std::uint32_t>("paint-a",bounds,first),patch<std::uint32_t>("paint-b",bounds,second),patch<double>("mask",bounds,maskValues)};
            const auto start=std::chrono::steady_clock::now();
            const auto r=history.apply(batch,p.reader(),p.writer(),"8K multi-plane region");
            latencies.push_back(micros(start));
            check(r.ok() && r.changed && r.affectedSamples==3*PatchWidth*PatchHeight,"8K region command failed");
        }
        const auto retained=history.statistics().retainedBytes;
        check(retained<3*1024*1024 && history.statistics().undoEntries==Edits,"8K history grows with full document size");
        const auto writes=p.writes;
        // A high-entropy full-frame replacement models a filter's candidate output.
        // Borrowing the other plane avoids allocating a third 256 MiB color image.
        const RasterHistoryPatch oversized{"paint-a",{{0,0},Side,Side},4,b.data};
        const auto rejectStart=std::chrono::steady_clock::now();
        const auto rejected=history.apply(std::span(&oversized,1),p.reader(),p.writer(),"oversized full-frame filter result");
        const auto rejectTime=micros(rejectStart);
        check(rejected.code==RasterHistoryCode::BudgetExceeded && p.writes==writes
            && history.statistics().retainedBytes==retained,"oversized 8K operation lost existing history");
        const auto undoStart=std::chrono::steady_clock::now();
        for(int i=0;i<Edits;++i) check(history.undo(p.reader(),p.writer()).ok(),"8K undo sequence failed");
        const auto undoTime=micros(undoStart);
        for(int edit=0;edit<Edits;++edit) {
            const auto x=(edit*137)%(Side-PatchWidth),y=(edit*223)%(Side-PatchHeight);
            for(int row=0;row<PatchHeight;++row) for(int column=0;column<PatchWidth;++column) {
                const auto index=static_cast<std::size_t>(y+row)*Side+x+column;
                check(p.at<std::uint32_t>("paint-a",x+column,y+row)==color(index,17)
                    && p.at<std::uint32_t>("paint-b",x+column,y+row)==color(index,37)
                    && p.at<double>("mask",x+column,y+row)==coverage(index),"8K undo pixels differ from the independent source pattern");
            }
        }
        for(int i=0;i<Edits;++i) check(history.redo(p.reader(),p.writer()).ok(),"8K redo sequence failed");
        std::sort(latencies.begin(),latencies.end());
        std::cout<<"RASTER_HISTORY_8K_OK width="<<Side<<" height="<<Side<<" planes=3 source_bytes="
            <<(a.data.size()+b.data.size()+mask.data.size())<<" commands="<<Edits
            <<" retained_bytes="<<retained<<" apply_median_us="<<latencies[latencies.size()/2]
            <<" apply_max_us="<<latencies.back()<<" rejection_us="<<rejectTime
            <<" undo_total_us="<<undoTime<<" writer_calls="<<p.writes<<'\n';
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
