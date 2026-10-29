#pragma once
#include <History/RasterEditHistory.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace historytest {
inline void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
struct Plane {
    int width = 0, height = 0;
    unsigned sampleBytes = 4;
    std::vector<std::byte> data;
};
struct Planes {
    std::map<std::string, Plane, std::less<>> values;
    bool rejectRead = false, rejectWrite = false;
    std::size_t writes = 0;
    bool read(std::string_view id, DevicePixelPoint first, unsigned sampleBytes,
              std::span<std::byte> output) const
    {
        if (rejectRead) return false;
        const auto found = values.find(id);
        if (found == values.end()) return false;
        const auto& p = found->second;
        const auto count = output.size() / sampleBytes;
        if (sampleBytes != p.sampleBytes || first.x < 0 || first.y < 0 || first.y >= p.height
            || count > static_cast<std::size_t>(p.width) || first.x > p.width - static_cast<int>(count)) return false;
        std::memcpy(output.data(), p.data.data() + (static_cast<std::size_t>(first.y) * p.width + first.x) * sampleBytes, output.size());
        return true;
    }
    bool write(std::span<const RasterHistoryPatch> patches)
    {
        if (rejectWrite) return false;
        // Validate the complete command before the first mutation.
        for (const auto& patch : patches) {
            const auto found = values.find(patch.plane);
            if (found == values.end()) return false;
            const auto& p = found->second;
            const auto& b = patch.bounds;
            if (patch.sampleBytes != p.sampleBytes || b.origin.x < 0 || b.origin.y < 0
                || b.width <= 0 || b.height <= 0
                || static_cast<std::int64_t>(b.origin.x) + b.width > p.width
                || static_cast<std::int64_t>(b.origin.y) + b.height > p.height) return false;
        }
        ++writes;
        for (const auto& patch : patches) {
            auto& p = values.find(patch.plane)->second;
            const auto& b = patch.bounds;
            for (int y = 0; y < b.height; ++y)
                std::memcpy(p.data.data() + (static_cast<std::size_t>(b.origin.y+y)*p.width+b.origin.x)*p.sampleBytes,
                    patch.data.data()+static_cast<std::size_t>(y)*b.width*p.sampleBytes,
                    static_cast<std::size_t>(b.width)*p.sampleBytes);
        }
        return true;
    }
    RasterEditHistory::Reader reader()
    {
        return [this](std::string_view id, DevicePixelPoint point, unsigned bytes, std::span<std::byte> output) {
            return read(id, point, bytes, output);
        };
    }
    RasterEditHistory::Writer writer()
    { return [this](std::span<const RasterHistoryPatch> patches) { return write(patches); }; }
    void add(std::string id, int width, int height, unsigned bytes = 4)
    { values.emplace(std::move(id), Plane{width,height,bytes,std::vector<std::byte>(static_cast<std::size_t>(width)*height*bytes)}); }
    template<class T> T at(std::string_view id, int x, int y) const
    {
        const auto& p=values.find(id)->second;T value{};
        std::memcpy(&value,p.data.data()+(static_cast<std::size_t>(y)*p.width+x)*p.sampleBytes,sizeof(T));
        return value;
    }
};
template<class T>
RasterHistoryPatch patch(std::string_view id, DevicePixelRect bounds, std::span<const T> values)
{ return {id,bounds,sizeof(T),std::as_bytes(values)}; }
}
