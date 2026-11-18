#pragma once
#include "../RenderControls.h"
#include <cmath>

namespace synth1x::render
{
    inline float filterControlIndex(const RenderControls& p) noexcept
    {return std::trunc(p.cutoffIndex)+p.filterCutoffAdjustment;}
}
