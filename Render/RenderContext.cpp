//
// Created by Justmoong on 2026 May 24.
//

#include "RenderContext.h"

bool renderColorSpacesMatch(const ColorSpace &source, const ColorSpace &target)
{
    return colorSpacesEquivalent(source, target);
}
