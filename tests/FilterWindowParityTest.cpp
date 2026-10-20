#include "Filter/FilterPipeline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {
DrawingSurface reference(DrawingSurface surface, const SelectionState &selection, double radius)
{
    const auto source = surface.pixels;
    const int r = std::max(1, static_cast<int>(std::lround(radius)));
    for (int y = 0; y < surface.height; ++y) {
        for (int x = 0; x < surface.width; ++x) {
            if (selectionAlphaAt(selection, {x, y}) <= 0) continue;
            std::array<std::uint64_t, 4> sum{};
            std::uint64_t count = 0;
            for (int yy = std::max(0, y-r); yy <= std::min(surface.height-1, y+r); ++yy) {
                for (int xx = std::max(0, x-r); xx <= std::min(surface.width-1, x+r); ++xx) {
                    auto p = source[yy*surface.width+xx];
                    for (unsigned c=0; c<4; ++c) sum[c] += (p >> (8*c)) & 255U;
                    ++count;
                }
            }
            std::uint32_t value = 0;
            for (unsigned c=0; c<4; ++c) value |= static_cast<std::uint32_t>(sum[c]/count) << (8*c);
            surface.pixels[y*surface.width+x] = value;
        }
    }
    return surface;
}
}
int main()
{
    std::uint32_t seed=11;
    for (int w : {0,1,2,9,31}) for (int h : {0,1,3,17}) {
        auto source=makeDrawingSurface(w,h);
        for (auto &p:source.pixels) {seed=seed*1664525U+1013904223U;p=seed;}
        std::array<SelectionState,4> selections{};
        selections[1]=makeRectangularSelection({{1.0,1.0},3.0,4.0});
        selections[2]=selections[1];selections[2].inverted=true;
        selections[3].active=true;selections[3].shape=SelectionShape::Mask;
        selections[3].mask.bounds={{0,0},w,h};selections[3].mask.alpha.resize(source.pixels.size());
        for (std::size_t i=0;i<source.pixels.size();++i) selections[3].mask.alpha[i]=i%3 ? 128 : 0;
        for (double r : {-1.0,0.0,1.0,1.49,1.5,4.0,40.0}) for (const auto &selection:selections) {
            auto actual=source;
            applyFilterPipeline(actual,selection,{{{true,FilterKind::Blur,r,1.0}}});
            if (actual.pixels != reference(source,selection,r).pixels) {
                std::cerr << "box window parity failed " << w << ' ' << h << ' ' << r << '\n';return 1;
            }
        }
    }
    return 0;
}
