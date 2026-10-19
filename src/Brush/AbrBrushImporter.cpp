#include "Brush/AbrBrushImporter.h"
#include "Brush/BrushResolve.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <numbers>
#include <utility>

namespace {
using Code = AbrImportErrorCode;
[[noreturn]] void fail(Code code, std::size_t offset, std::string message)
{
    throw AbrImportError{code,offset,std::move(message)};
}

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes, std::size_t origin=0)
        : bytes_(bytes), origin_(origin) {}
    std::size_t offset() const { return origin_+position_; }
    std::size_t remaining() const { return bytes_.size()-position_; }
    std::span<const std::byte> take(std::size_t n) {
        if(n>remaining()) fail(Code::TruncatedData,offset(),"Truncated ABR record.");
        const auto result=bytes_.subspan(position_,n);position_+=n;return result;
    }
    std::uint8_t u8() { return std::to_integer<std::uint8_t>(take(1).front()); }
    std::uint16_t u16() { const auto high=u8();return static_cast<std::uint16_t>((high<<8)|u8()); }
    std::uint32_t u32() { const auto high=u16();return (std::uint32_t(high)<<16)|u16(); }
    std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }
    std::string text(std::size_t count) {
        const auto data=take(count);
        std::string result; result.reserve(count);
        for(auto value:data) result.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
        return result;
    }
private:
    std::span<const std::byte> bytes_;
    std::size_t origin_=0,position_=0;
};

void utf8(std::string &text, std::uint32_t cp)
{
    if(cp<0x80) text.push_back(static_cast<char>(cp));
    else if(cp<0x800) {
        text.push_back(static_cast<char>(0xC0|(cp>>6)));
        text.push_back(static_cast<char>(0x80|(cp&63)));
    } else if(cp<0x10000) {
        text.push_back(static_cast<char>(0xE0|(cp>>12)));
        text.push_back(static_cast<char>(0x80|((cp>>6)&63)));
        text.push_back(static_cast<char>(0x80|(cp&63)));
    } else {
        text.push_back(static_cast<char>(0xF0|(cp>>18)));
        text.push_back(static_cast<char>(0x80|((cp>>12)&63)));
        text.push_back(static_cast<char>(0x80|((cp>>6)&63)));
        text.push_back(static_cast<char>(0x80|(cp&63)));
    }
}
std::string unicodeName(Reader &reader)
{
    const auto count=reader.u32();
    if(count>4096) fail(Code::ResourceLimit,reader.offset(),"ABR name exceeds 4096 UTF-16 units.");
    const auto origin=reader.offset();
    Reader units(reader.take(std::size_t(count)*2),origin);
    std::string name;
    for(std::uint32_t i=0;i<count;++i) {
        std::uint32_t cp=units.u16();
        if(cp>=0xD800 && cp<=0xDBFF) {
            if(++i>=count) fail(Code::InvalidData,units.offset(),"Unpaired UTF-16 high surrogate.");
            const auto low=units.u16();
            if(low<0xDC00 || low>0xDFFF) fail(Code::InvalidData,units.offset(),"Invalid UTF-16 surrogate pair.");
            cp=0x10000+((cp-0xD800)<<10)+(low-0xDC00);
        } else if(cp>=0xDC00 && cp<=0xDFFF) {
            fail(Code::InvalidData,units.offset(),"Unpaired UTF-16 low surrogate.");
        }
        utf8(name,cp);
    }
    while(!name.empty() && name.back()=='\0') name.pop_back();
    if(name.size()>8192) fail(Code::ResourceLimit,origin,"Decoded ABR name exceeds the brush name limit.");
    return name;
}

PaintUuid identifier(std::span<const std::byte> record, std::size_t ordinal)
{
    // Stable local IDs, not authentication hashes or copied Adobe UUIDs.
    std::uint64_t a=14695981039346656037ULL, b=1099511628211ULL^ordinal;
    for(auto byte:record) {
        const auto value=std::to_integer<std::uint8_t>(byte);
        a=(a^value)*1099511628211ULL;
        b=(b^(value+ordinal))*1099511628211ULL;
    }
    PaintUuid id;
    for(std::size_t i=0;i<8;++i) {
        id.bytes[i]=static_cast<std::uint8_t>(a>>(i*8));
        id.bytes[i+8]=static_cast<std::uint8_t>(b>>(i*8));
    }
    id.bytes[6]=(id.bytes[6]&15)|0x80;
    id.bytes[8]=(id.bytes[8]&63)|0x80;
    return id;
}

struct Decoder {
    const AbrImportLimits &limits;
    AbrBrushImport output;
    std::size_t decodedBytes=0;

    void append(AbrImportedBrush brush, std::span<const std::byte> record, std::size_t offset) {
        if(output.brushes.size()>=limits.maxBrushes)
            fail(Code::ResourceLimit,offset,"ABR brush count exceeds the configured limit.");
        brush.preset.brushId=identifier(record,output.brushes.size()+1);
        const auto errors=validateBrushPreset(brush.preset);
        if(!errors.empty()) fail(Code::InvalidData,offset,"Imported ABR brush is invalid: "+errors.front());
        if(brush.preset.size<=0) fail(Code::InvalidData,offset,"ABR computed tip must have a positive diameter.");
        output.brushes.push_back(std::move(brush));
    }

    void sampled(Reader &reader, AbrImportedBrush &brush) {
        const auto top=reader.i32(),left=reader.i32(),bottom=reader.i32(),right=reader.i32();
        const auto width=std::int64_t(right)-left,height=std::int64_t(bottom)-top;
        if(width<=0 || height<=0) fail(Code::InvalidData,reader.offset(),"ABR sampled tip has empty or inverted bounds.");
        if(width>4096 || height>4096) fail(Code::ResourceLimit,reader.offset(),"ABR tip exceeds 4096 pixels per edge.");
        const auto pixels=static_cast<std::size_t>(width*height);
        if(pixels>limits.maxTotalMaskBytes-decodedBytes)
            fail(Code::ResourceLimit,reader.offset(),"ABR sampled masks exceed the decoded byte budget.");
        brush.sourceDepth=reader.u16();
        if(brush.sourceDepth!=8 && brush.sourceDepth!=16)
            fail(Code::UnsupportedDepth,reader.offset(),"ABR tip depth must be 8 or 16 bits.");
        const auto compression=reader.u8();
        if(compression>1) fail(Code::UnsupportedCompression,reader.offset(),"ABR tip compression must be raw or PackBits.");
        const auto stride=static_cast<std::size_t>(width)*(brush.sourceDepth/8);
        const auto rows=static_cast<std::size_t>(height);
        if(compression==0 && stride*rows>reader.remaining())
            fail(Code::TruncatedData,reader.offset(),"Truncated raw ABR tip pixels.");
        std::vector<std::uint16_t> rowLengths;
        if(compression==1) {
            rowLengths.reserve(rows);
            std::size_t total=0;
            for(std::size_t y=0;y<rows;++y) { const auto size=reader.u16();rowLengths.push_back(size);total+=size; }
            if(total>reader.remaining()) fail(Code::TruncatedData,reader.offset(),"Truncated ABR PackBits rows.");
        }
        auto &tip=brush.preset.tip;
        tip.width=static_cast<int>(width); tip.height=static_cast<int>(height); tip.mask.resize(pixels);
        std::vector<std::byte> unpacked(stride);
        for(std::size_t y=0;y<rows;++y) {
            std::span<const std::byte> row;
            if(compression==0) row=reader.take(stride);
            else {
                const auto position=reader.offset();
                Reader packed(reader.take(rowLengths[y]),position);
                std::size_t written=0;
                while(packed.remaining()) {
                    const auto control=packed.u8();
                    if(control==128) continue;
                    const std::size_t count=control<128 ? control+1 : 257-control;
                    if(count>stride-written) fail(Code::InvalidData,packed.offset(),"ABR PackBits packet exceeds its scanline.");
                    if(control<128) {
                        const auto values=packed.take(count);
                        std::copy(values.begin(),values.end(),unpacked.begin()+static_cast<std::ptrdiff_t>(written));
                    } else {
                        const auto value=std::byte(packed.u8());
                        std::fill_n(unpacked.begin()+static_cast<std::ptrdiff_t>(written),count,value);
                    }
                    written+=count;
                }
                if(written!=stride) fail(Code::InvalidData,packed.offset(),"ABR PackBits row is shorter than its scanline.");
                row=unpacked;
            }
            for(std::size_t x=0;x<static_cast<std::size_t>(width);++x) {
                unsigned value=std::to_integer<unsigned>(row[x*(brush.sourceDepth/8)]);
                if(brush.sourceDepth==16)
                    value=(((value<<8)|std::to_integer<unsigned>(row[x*2+1]))+128U)/257U;
                tip.mask[y*static_cast<std::size_t>(width)+x]=std::byte(value);
            }
        }
        decodedBytes+=pixels;
        brush.preset.size=static_cast<float>(std::max(width,height));
        brush.preset.shape.angleMode=BrushAngleMode::Fixed;
        if(brush.sourceDepth==16)
            output.warnings.push_back("16-bit ABR tip coverage was rounded to the engine's 8-bit mask.");
    }

    void legacy(Reader &reader) {
        const auto count=reader.u16();
        if(count>limits.maxBrushes) fail(Code::ResourceLimit,reader.offset(),"ABR brush count exceeds the configured limit.");
        for(std::size_t i=0;i<count;++i) {
            const auto type=reader.u16();const auto length=reader.u32();const auto start=reader.offset();
            const auto record=reader.take(length);Reader body(record,start);
            if(type!=1 && type!=2) {
                output.warnings.push_back("Unsupported legacy ABR brush type "+std::to_string(type)+" was skipped.");
                continue;
            }
            AbrImportedBrush brush;
            const auto misc=body.u32(); // Legacy miscellaneous flags, not brush dynamics.
            const auto spacing=body.u16();
            brush.preset.stroke.spacingEnabled=spacing!=0;
            brush.preset.stroke.spacingRatio=spacing/100.0;
            brush.preset.name="ABR brush "+std::to_string(i+1);
            brush.completeSettings=misc==0;
            if(misc) output.warnings.push_back("Nonzero legacy ABR miscellaneous flags were not mapped.");
            if(type==1) {
                brush.preset.size=body.u16();brush.preset.shape.roundness=body.u16()/100.0;
                brush.preset.shape.angleRadians=std::bit_cast<std::int16_t>(body.u16())*std::numbers::pi/180;
                brush.preset.hardness=body.u16()/100.0F;brush.preset.shape.angleMode=BrushAngleMode::Fixed;
            } else {
                if(output.version==2) {
                    auto name=unicodeName(body);if(!name.empty()) brush.preset.name=std::move(name);
                }
                if(!body.u8()) {
                    brush.completeSettings=false;
                    output.warnings.push_back("Legacy ABR anti-alias-off is not represented by the current mask sampler.");
                }
                body.take(8); // Superseded short bounds.
                sampled(body,brush);
            }
            if(body.remaining()) {
                brush.completeSettings=false;
                output.warnings.push_back("Unused trailing bytes in legacy ABR brush "+std::to_string(i+1)+".");
            }
            append(std::move(brush),record,start);
        }
    }

    void samples(Reader &reader) {
        while(reader.remaining()) {
            const auto length=reader.u32();const auto start=reader.offset();
            if(length==0) {
                const auto padding=reader.take(reader.remaining());
                if(std::ranges::any_of(padding,[](auto b){return b!=std::byte{0};}))
                    fail(Code::InvalidData,start,"Nonzero data after ABR sample terminator.");
                break;
            }
            const auto record=reader.take(length);Reader body(record,start);
            AbrImportedBrush brush;
            const auto idLength=body.u8();
            brush.sourceId=body.text(idLength);
            body.take(output.subversion==1 ? 10 : 264);
            brush.preset.name="ABR tip "+(brush.sourceId.empty()?std::to_string(output.brushes.size()+1):brush.sourceId);
            sampled(body,brush);
            append(std::move(brush),record,start);
            reader.take((4-length%4)%4);
        }
    }

    void modern(Reader &reader) {
        output.subversion=reader.u16();
        if(output.subversion!=1 && output.subversion!=2)
            fail(Code::UnsupportedVersion,reader.offset(),"ABR sample subversion must be 1 or 2.");
        output.warnings.push_back("Modern ABR import extracts sampled tips only; preset descriptors, patterns and folders are not restored.");
        unsigned sections=0;
        while(reader.remaining()) {
            if(++sections>4096) fail(Code::ResourceLimit,reader.offset(),"ABR exceeds 4096 sections.");
            if(reader.text(4)!="8BIM") fail(Code::InvalidData,reader.offset(),"Invalid ABR section signature.");
            const auto key=reader.text(4);
            const auto length=reader.u32();const auto start=reader.offset();
            Reader section(reader.take(length),start);
            if(key=="samp") samples(section);
            else output.warnings.push_back("ABR section "+key+" was not imported.");
            reader.take((4-length%4)%4);
        }
    }
};
}

std::expected<AbrBrushImport,AbrImportError> importAbrBrushes(
        std::span<const std::byte> bytes, const AbrImportLimits &limits)
{
    try {
        if(limits.maxBrushes==0 || limits.maxBrushes>20000
                || limits.maxFileBytes==0 || limits.maxFileBytes>256*1024*1024
                || limits.maxTotalMaskBytes==0 || limits.maxTotalMaskBytes>256*1024*1024)
            fail(Code::ResourceLimit,0,"Invalid ABR import limits.");
        if(bytes.size()>limits.maxFileBytes) fail(Code::ResourceLimit,0,"ABR file exceeds the encoded byte limit.");
        Reader reader(bytes);Decoder decoder{limits,{},0};
        decoder.output.version=reader.u16();
        switch(decoder.output.version) {
            case 1: case 2: decoder.legacy(reader);break;
            case 6: case 7: case 9: case 10: decoder.modern(reader);break;
            default: fail(Code::UnsupportedVersion,0,"Unsupported ABR version.");
        }
        if(reader.remaining()) fail(Code::InvalidData,reader.offset(),"Trailing data after the ABR brush collection.");
        if(decoder.output.brushes.empty()) fail(Code::UnsupportedContent,reader.offset(),"ABR contains no supported computed or sampled tips.");
        return std::move(decoder.output);
    } catch(const AbrImportError &error) { return std::unexpected(error); }
    catch(const std::bad_alloc &) { return std::unexpected(AbrImportError{Code::ResourceLimit,0,"ABR allocation failed."}); }
}
