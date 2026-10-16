#include "LocalMaskBrush.h"
#include <cmath>
#include <initializer_list>

bool isValidLocalMaskBrushOptions(const LocalMaskBrushOptions &options)
{
    if (!std::isfinite(options.sizePixels) || options.sizePixels < 0.01 || options.sizePixels > 4096
            || (options.mode != MaskBrushMode::Add && options.mode != MaskBrushMode::Erase)) return false;
    for (auto value : {options.feather,options.flow,options.density,options.strokeOpacity,options.autoMaskTolerance})
        if (!std::isfinite(value) || value < 0 || value > 1) return false;
    return true;
}
