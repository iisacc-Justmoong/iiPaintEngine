#include "Selection/Selection.h"
#include "Tool/RasterEditTool.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
void check(bool condition, const char* name)
{
    if (!condition) {
        std::cerr << name << '\n';
        ++failures;
    }
}
bool near(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-10;
}
}

int main()
{
    auto rectangle = makeRectangularSelection({{1, 1}, 5, 5});
    rectangle.inverted = true;
    check(selectionAlphaAt(rectangle, {2, 2}) == 0, "inverted rectangle inside");
    check(selectionAlphaAt(rectangle, {0, 0}) == 1, "inverted rectangle outside");
    rectangle.inverted = false;
    rectangle.featherRadius = 1;
    // A radius-one box has nine equally weighted lattice samples, with zero
    // coverage beyond the original selection. Expectations are hand counted.
    check(near(selectionAlphaAt(rectangle, {1, 1}), 4.0 / 9), "feather rectangle corner");
    check(near(selectionAlphaAt(rectangle, {1, 3}), 2.0 / 3), "feather rectangle edge");
    check(near(selectionAlphaAt(rectangle, {0, 3}), 1.0 / 3), "feather rectangle exterior");
    check(selectionAlphaAt(rectangle, {-1, 3}) == 0, "feather finite support");
    check(selectionAlphaAt(rectangle, {3, 3}) == 1, "feather rectangle interior");
    rectangle.inverted = true;
    check(near(selectionAlphaAt(rectangle, {1, 1}), 5.0 / 9), "invert after feather");

    SelectionState mask;
    mask.active = true;
    mask.shape = SelectionShape::Mask;
    mask.bounds = {{1, 1}, 3, 3};
    mask.mask.bounds = {{1, 1}, 3, 3};
    mask.mask.alpha.assign(9, 255);
    mask.mask.alpha[4] = 0;
    check(!selectionContains(mask, {2, 2}), "mask containment respects holes");
    check(selectionContains(mask, {1.8, 1.8}), "mask containment samples containing pixel");
    mask.mask.alpha[4] = 255;
    mask.featherRadius = 1;
    check(near(selectionAlphaAt(mask, {1, 1}), 4.0 / 9), "mask feather corner");
    check(near(selectionAlphaAt(mask, {0, 2}), 1.0 / 3), "mask feather exterior");
    mask.inverted = true;
    check(near(selectionAlphaAt(mask, {1, 1}), 5.0 / 9), "mask inversion once");
    SelectionState inactive;
    inactive.inverted = true;
    inactive.featherRadius = 3;
    check(selectionAlphaAt(inactive, {0, 0}) == 1, "inactive selection unrestricted");

    auto surface = makeDrawingSurface(3, 1, 0xff000000U);
    FillOperation fill;
    fill.argb = 0xffff0000U;
    fill.opacity = 0;
    applyFill(surface, {}, fill);
    check(surface.pixels[0] == 0xff000000U, "zero opacity fill preserves bytes");
    fill.opacity = .5;
    applyFill(surface, {}, fill);
    check(surface.pixels[0] == 0xff800000U, "half opacity fill");
    surface = makeDrawingSurface(3, 1, 0xff000000U);
    SelectionState soft;
    soft.active = true;
    soft.shape = SelectionShape::Mask;
    soft.bounds = {{0, 0}, 3, 1};
    soft.mask.bounds = {{0, 0}, 3, 1};
    soft.mask.alpha = {128, 0, 255};
    fill.opacity = 1;
    applyFill(surface, soft, fill);
    check(surface.pixels[0] == 0xff800000U, "fill fractional coverage");
    check(surface.pixels[1] == 0xff000000U, "fill zero coverage");
    check(surface.pixels[2] == 0xffff0000U, "fill full coverage");
    surface = makeDrawingSurface(3, 1, 0xff000000U);
    fill.opacity = .5;
    applyFill(surface, soft, fill);
    check(surface.pixels[0] == 0xff400000U, "opacity times selection coverage");
    surface = makeDrawingSurface(1, 1, 0x00123456U);
    fill.argb = 0x80ff0000U;
    fill.opacity = 1;
    applyFill(surface, {}, fill);
    check(surface.pixels[0] == 0x80ff0000U, "straight alpha source over transparent");
    surface = makeDrawingSurface(1, 1, 0xff0000ffU);
    applyFill(surface, {}, fill);
    check(surface.pixels[0] == 0xff80007fU, "straight alpha source over opaque");

    GradientOperation gradient;
    gradient.kind = GradientKind::Radial;
    gradient.start = {0, 0};
    gradient.end = {2, 0};
    gradient.startArgb = 0xffff0000U;
    gradient.endArgb = 0xff0000ffU;
    surface = makeDrawingSurface(4, 1, 0xff000000U);
    applyGradient(surface, {}, gradient);
    check(surface.pixels[0] == 0xffff0000U, "radial center");
    check(surface.pixels[1] == 0xff800080U, "radial midpoint");
    check(surface.pixels[2] == 0xff0000ffU, "radial radius");
    check(surface.pixels[3] == 0xff0000ffU, "radial clamped exterior");
    surface = makeDrawingSurface(3, 1, 0xff000000U);
    gradient.opacity = .5;
    applyGradient(surface, soft, gradient);
    check(surface.pixels[0] == 0xff400000U, "gradient opacity and soft mask");
    gradient.opacity = 0;
    const auto previous = surface.pixels;
    applyGradient(surface, {}, gradient);
    check(surface.pixels == previous, "zero opacity gradient");

    surface = makeDrawingSurface(3, 1, 0xff123456U);
    EraserOperation eraser;
    applyEraser(surface, soft, eraser);
    check(surface.pixels[0] == 0x7f123456U, "eraser fractional coverage");
    check(surface.pixels[1] == 0xff123456U, "eraser zero coverage");
    check(surface.pixels[2] == 0x00123456U, "eraser full coverage preserves hidden rgb");

    fill.opacity = std::numeric_limits<double>::quiet_NaN();
    surface = makeDrawingSurface(1, 1, 0x00123456U);
    applyFill(surface, {}, fill);
    check(surface.pixels[0] == 0x00123456U, "non-finite opacity is no-op");
    soft.mask.alpha = {128};
    check(selectionAlphaAt(soft, {2, 0}) == 0, "short mask storage outside bytes");
    mask.inverted = false;
    mask.featherRadius = 0;
    mask.mask.bounds = {{std::numeric_limits<Types::Pixel>::max(), 0}, 2, 1};
    mask.mask.alpha = {255, 0};
    check(selectionAlphaAt(mask, {std::numeric_limits<Types::Pixel>::max(), 0}) == 1,
          "mask bounds arithmetic does not overflow");
    surface = makeDrawingSurface(3, 1, 0xff000000U);
    gradient.opacity = 1;
    gradient.end = gradient.start;
    applyGradient(surface, {}, gradient);
    check(surface.pixels[2] == gradient.startArgb, "degenerate radial uses first color");
    gradient.start = {1e200, 0};
    gradient.end = {-1e200, 0};
    gradient.kind = GradientKind::Linear;
    surface = makeDrawingSurface(1, 1, 0xff000000U);
    applyGradient(surface, {}, gradient);
    check(surface.pixels[0] == 0xff800080U, "linear huge finite span is stable");
    gradient.start = {std::numeric_limits<double>::max(), 0};
    gradient.end = {-std::numeric_limits<double>::max(), 0};
    const auto preserved = surface.pixels;
    applyGradient(surface, {}, gradient);
    check(surface.pixels == preserved, "overflowed gradient span no-op");
    auto truncated = makeDrawingSurface(2, 2, 0xff000000U);
    truncated.pixels.resize(1);
    fill.opacity = 1;
    fill.argb = 0xffff0000U;
    applyFill(truncated, {}, fill);
    check(truncated.pixels.size() == 1 && truncated.pixels[0] == fill.argb,
          "partial surface storage is bounded");
    if (failures == 0) std::cout << "Selection raster edit contract passed\n";
    return failures == 0 ? 0 : 1;
}
