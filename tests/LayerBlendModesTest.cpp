#include "Render/Compositor.h"
#include <array>
#include <cstdint>
#include <iostream>

int main()
{
    const std::array modes{
        RasterBlendMode::SourceOver, RasterBlendMode::Dissolve, RasterBlendMode::Darken,
        RasterBlendMode::Multiply, RasterBlendMode::ColorBurn, RasterBlendMode::LinearBurn,
        RasterBlendMode::DarkerColor, RasterBlendMode::Lighten, RasterBlendMode::Screen,
        RasterBlendMode::ColorDodge, RasterBlendMode::LinearDodge, RasterBlendMode::LighterColor,
        RasterBlendMode::Overlay, RasterBlendMode::SoftLight, RasterBlendMode::HardLight,
        RasterBlendMode::VividLight, RasterBlendMode::LinearLight, RasterBlendMode::PinLight,
        RasterBlendMode::HardMix, RasterBlendMode::Difference, RasterBlendMode::Exclusion,
        RasterBlendMode::Subtract, RasterBlendMode::Divide, RasterBlendMode::Hue,
        RasterBlendMode::Saturation, RasterBlendMode::Color, RasterBlendMode::Luminosity};
    for (auto mode : modes) {
        if (compositeRasterPixel(0xff804020, 0, mode) != 0xff804020
            || compositeRasterPixel(0xff804020, 0x80607080, mode, 0) != 0x80607080) {
            std::cerr << "transparent backdrop/zero opacity failed for " << int(mode) << '\n';
            return 1;
        }
    }
    // Independent opaque reference values, including burn/dodge endpoint ordering.
    struct Case { RasterBlendMode mode; std::uint32_t source, backdrop, expected; };
    const std::array cases{
        Case{RasterBlendMode::Darken, 0xffc02040, 0xff408060, 0xff402040},
        Case{RasterBlendMode::Lighten, 0xffc02040, 0xff408060, 0xffc08060},
        Case{RasterBlendMode::LinearBurn, 0xff804020, 0xff808080, 0xff010000},
        Case{RasterBlendMode::LinearDodge, 0xff804020, 0xff808080, 0xffffc0a0},
        Case{RasterBlendMode::Difference, 0xff804020, 0xff408060, 0xff404040},
        Case{RasterBlendMode::Subtract, 0xff804020, 0xff408060, 0xff004040},
        Case{RasterBlendMode::Divide, 0xff808080, 0xff402010, 0xff804020},
        Case{RasterBlendMode::ColorDodge, 0xffffffff, 0xff000000, 0xff000000},
        Case{RasterBlendMode::ColorBurn, 0xff000000, 0xffffffff, 0xffffffff},
        Case{RasterBlendMode::HardMix, 0xff808080, 0xff404040, 0xff000000},
        Case{RasterBlendMode::DarkerColor, 0xfff00000, 0xff008000, 0xff008000},
        Case{RasterBlendMode::LighterColor, 0xfff00000, 0xff008000, 0xfff00000},
        Case{RasterBlendMode::Color, 0xffff0000, 0xff808080, 0xffff4a4a},
        Case{RasterBlendMode::Luminosity, 0xff808080, 0xffff0000, 0xffff4a4a},
        Case{RasterBlendMode::SoftLight, 0xff000000, 0xff404040, 0xff101010},
        Case{RasterBlendMode::SoftLight, 0xffffffff, 0xff404040, 0xff808080},
        Case{RasterBlendMode::HardLight, 0xffc0c0c0, 0xff404040, 0xffa1a1a1},
        Case{RasterBlendMode::VividLight, 0xffc0c0c0, 0xff404040, 0xff828282},
        Case{RasterBlendMode::LinearLight, 0xffc0c0c0, 0xff404040, 0xffc1c1c1},
        Case{RasterBlendMode::PinLight, 0xffc0c0c0, 0xff404040, 0xff818181},
        Case{RasterBlendMode::Exclusion, 0xff808080, 0xff404040, 0xff808080},
        Case{RasterBlendMode::Hue, 0xffff0000, 0xff0000ff, 0xff5e0000},
        Case{RasterBlendMode::Saturation, 0xff808080, 0xffff0000, 0xff4d4d4d},
    };
    for (const auto &c : cases) {
        const auto actual = compositeRasterPixel(c.source, c.backdrop, c.mode);
        if (actual != c.expected) {
            std::cerr << "mode " << int(c.mode) << ": " << std::hex << actual
                      << " != " << c.expected << '\n'; return 2;
        }
    }
    // W3C overlap weighting: only the overlapped part is multiplied.
    if (compositeRasterPixel(0x80ff0000, 0x800000ff, RasterBlendMode::Multiply) != 0xc0550055)
        return 3;
    if (compositeRasterPixel(0x80ff0000, 0xff123456, RasterBlendMode::DestinationOut) != 0x7f123456)
        return 4;
    unsigned selected = 0;
    for (std::uint32_t i = 0; i < 4096; ++i) {
        const auto p = compositeRasterPixel(0x80ff0000, 0, RasterBlendMode::Dissolve, 1, i);
        if (p != 0 && p != 0xffff0000) return 5;
        selected += p != 0;
        if (p != compositeRasterPixel(0x80ff0000, 0, RasterBlendMode::Dissolve, 1, i)) return 6;
    }
    if (selected < 1850 || selected > 2250) return 7;
    for (unsigned s = 0; s < 256; ++s) for (unsigned b = 0; b < 256; ++b) {
        const auto expected = s+b >= 255 ? 0xffffffffU : 0xff000000U;
        if (compositeRasterPixel(0xff000000U | s*0x010101U, 0xff000000U | b*0x010101U,
                                 RasterBlendMode::HardMix) != expected) return 10;
    }
    LayerStack clipping;
    Layer background; background.surface = makeDrawingSurface(2,1,0xff0000ff);
    clipping.layers.push_back(background);
    Layer base; base.surface = makeDrawingSurface(2,1,0);
    base.surface.pixels[0] = 0x8000ff00; clipping.layers.push_back(base);
    Layer top; top.surface = makeDrawingSurface(2,1,0xffff0000);
    top.metadata.clipsToBelow = true; clipping.layers.push_back(top);
    const auto clipped = compositeLayerStack(clipping,2,1);
    if (clipped.pixels[0] != 0xff80007f || clipped.pixels[1] != 0xff0000ff) return 8;
    clipping.layers[1].metadata.visible = false;
    if (compositeLayerStack(clipping,2,1).pixels[0] != 0xff0000ff) return 9;
    return 0;
}
