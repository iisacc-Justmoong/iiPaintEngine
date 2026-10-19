#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "Brush/AbrBrushImporter.h"
#include "Brush/BrushResolve.h"
#include "Brush/BrushPresetSerializer.h"
#include "Stroke/Rasterizer.h"

#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " #c "\n"; return 1; } } while (false)
namespace {
using Bytes=std::vector<std::byte>;
void byte(Bytes &b,unsigned v){b.push_back(std::byte(v));}
void u16(Bytes &b,unsigned v){byte(b,v>>8);byte(b,v);}
void u32(Bytes &b,std::uint32_t v){u16(b,v>>16);u16(b,v);}
void text(Bytes &b,const std::string &s){for(unsigned char c:s)byte(b,c);}
void append(Bytes &b,const Bytes &v){b.insert(b.end(),v.begin(),v.end());}
void record(Bytes &b,unsigned type,const Bytes &body){u16(b,type);u32(b,body.size());append(b,body);}
void sample(Bytes &b,unsigned depth,unsigned compression,const Bytes &pixels,unsigned w=2,unsigned h=2){
    u32(b,0);u32(b,0);u32(b,h);u32(b,w);u16(b,depth);byte(b,compression);append(b,pixels);
}
Bytes legacy(bool rle=false) {
    Bytes b;u16(b,2);u16(b,2);
    Bytes computed;u32(computed,0);u16(computed,25);u16(computed,12);u16(computed,50);
    u16(computed,static_cast<std::uint16_t>(-30));u16(computed,75);record(b,1,computed);
    Bytes tip;u32(tip,0);u16(tip,20);u32(tip,4);u16(tip,0xBD93);u16(tip,0xD83D);u16(tip,0xDE00);u16(tip,0);
    byte(tip,1);for(int i=0;i<4;++i)u16(tip,0);
    Bytes pixels;
    if(rle){u16(pixels,3);u16(pixels,3);byte(pixels,1);byte(pixels,0);byte(pixels,255);byte(pixels,1);byte(pixels,128);byte(pixels,64);}
    else pixels={std::byte{0},std::byte{255},std::byte{128},std::byte{64}};
    sample(tip,8,rle?1:0,pixels);record(b,2,tip);return b;
}
Bytes modern(unsigned version,unsigned subversion,unsigned depth,bool rle) {
    Bytes entry;byte(entry,3);text(entry,"tip");entry.resize(entry.size()+(subversion==1?10:264));
    Bytes pixels;
    if(depth==8){if(rle){u16(pixels,3);byte(pixels,1);byte(pixels,0);byte(pixels,255);}else{byte(pixels,0);byte(pixels,255);}}
    else {if(rle){u16(pixels,5);byte(pixels,3);} byte(pixels,0);byte(pixels,0);byte(pixels,255);byte(pixels,255);}
    sample(entry,depth,rle?1:0,pixels,2,1);
    Bytes section;u32(section,entry.size());append(section,entry);while(section.size()%4)byte(section,0);
    Bytes b;u16(b,version);u16(b,subversion);text(b,"8BIMsamp");u32(b,section.size());append(b,section);
    return b;
}
}
int main()
{
    const auto data=legacy();
    auto imported=importAbrBrushes(data);
    CHECK(imported && imported->brushes.size()==2 && imported->version==2);
    const auto &computed=imported->brushes[0].preset;
    CHECK(computed.size==12 && computed.hardness==.75F && computed.shape.roundness==.5);
    CHECK(computed.stroke.spacingRatio==.25 && computed.tip.mask.empty());
    const auto &sampled=imported->brushes[1].preset;
    CHECK(sampled.name=="붓😀" && sampled.tip.width==2 && sampled.tip.height==2);
    CHECK(sampled.tip.mask==std::vector<std::byte>({std::byte{0},std::byte{255},std::byte{128},std::byte{64}}));
    CHECK(std::ranges::any_of(sampled.brushId.bytes,[](auto v){return v!=0;}));
    CHECK(sampled.brushId.bytes==importAbrBrushes(data)->brushes[1].preset.brushId.bytes);
    CHECK(sampled.brushId.bytes!=computed.brushId.bytes);
    CHECK(importAbrBrushes(legacy(true))->brushes[1].preset.tip.mask==sampled.tip.mask);
    Bytes v1;u16(v1,1);u16(v1,1);
    Bytes computedBody;u32(computedBody,0);u16(computedBody,25);u16(computedBody,8);
    u16(computedBody,100);u16(computedBody,0);u16(computedBody,100);record(v1,1,computedBody);
    CHECK(importAbrBrushes(v1) && importAbrBrushes(v1)->brushes.front().preset.size==8);
    CHECK(readBrushPreset(serializeBrushPreset(sampled)).preset);
    const auto brush=resolveBrushPreset(sampled,0xFF336699U);
    CHECK(brush.brush);RasterDabStream stream;
    CHECK(!projectBrushDabs(appendRasterDabs(stream,StrokePoint{{8,8}},*brush.brush),brush.brush->rasterizer).empty());
    for(unsigned version:{6U,7U,9U,10U}) for(unsigned subversion:{1U,2U})
        for(unsigned depth:{8U,16U}) for(bool rle:{false,true}) {
            const auto file=modern(version,subversion,depth,rle);
            const auto result=importAbrBrushes(file);
            CHECK(result && result->brushes.size()==1 && result->subversion==subversion);
            CHECK(result->brushes[0].sourceId=="tip" && !result->brushes[0].completeSettings);
            CHECK(!result->warnings.empty() && result->brushes[0].sourceDepth==depth);
            CHECK(result->brushes[0].preset.tip.mask==std::vector<std::byte>({std::byte{0},std::byte{255}}));
        }
    // Known records fail atomically on truncated input or malformed compression.
    for(std::size_t size=0;size<data.size();++size)
        CHECK(!importAbrBrushes(std::span(data).first(size)));
    AbrImportLimits limits;limits.maxBrushes=1;
    const auto tooMany=importAbrBrushes(data,limits);
    CHECK(!tooMany && tooMany.error().code==AbrImportErrorCode::ResourceLimit);
    limits={};limits.maxTotalMaskBytes=3;
    CHECK(!importAbrBrushes(data,limits));
    auto bad=modern(6,1,32,false);CHECK(!importAbrBrushes(bad));
    bad=modern(6,1,8,true);bad[bad.size()-5]=std::byte{127};CHECK(!importAbrBrushes(bad));
    auto repeated=modern(6,1,8,true);
    repeated[repeated.size()-5]=std::byte{128}; // no-op
    repeated[repeated.size()-4]=std::byte{255}; // repeat two bytes
    repeated[repeated.size()-3]=std::byte{128};
    const auto runs=importAbrBrushes(repeated);
    CHECK(runs && runs->brushes.front().preset.tip.mask==std::vector<std::byte>({std::byte{128},std::byte{128}}));
    auto half=modern(6,1,16,false);
    half[half.size()-5]=std::byte{128};half[half.size()-4]=std::byte{0};
    CHECK(importAbrBrushes(half)->brushes.front().preset.tip.mask.back()==std::byte{128});
    auto sections=modern(6,1,8,false);text(sections,"8BIMdesc");u32(sections,4);u32(sections,16);
    const auto partial=importAbrBrushes(sections);
    CHECK(partial && !partial->brushes.front().completeSettings && partial->warnings.size()>=2);
    limits={};limits.maxFileBytes=1;
    CHECK(!importAbrBrushes(data,limits));
    // Mutation smoke coverage is bounded; accepted data must still resolve as valid brushes.
    for(std::size_t i=0;i<data.size();++i) {
        auto mutated=data;mutated[i]^=std::byte{0xA5};
        const auto result=importAbrBrushes(mutated);
        if(result)for(const auto &entry:result->brushes)CHECK(resolveBrushPreset(entry.preset).brush);
    }
    return 0;
}
