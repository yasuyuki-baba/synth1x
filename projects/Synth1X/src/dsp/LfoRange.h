#pragma once
#include "ModulationSettings.h"
#include <cmath>
#include <algorithm>
#include <utility>

namespace synth1x::dsp
{
    // CLFO 0x15080, native receiver ranges and admitted integer depth0..127.
    // Integer control knots retain source-word rounding; real receiver buffers
    // interpolate these values in Float32 without changing RNG advancement.
    // Range recalculation does not redraw the random target or move its history.
    inline std::pair<int,int> lfoDepthRange(LfoDestination destination,int depth) noexcept
    {
        float amount;
        if(destination==LfoDestination::Oscillator2 || destination==LfoDestination::BothOscillators)
        {
            const float normalized=static_cast<float>(depth)*(1.0f/127.0f);
            amount=static_cast<float>((std::pow(10.0,static_cast<double>(normalized))-1.0)*(1.0/9.0));
        }
        else amount=static_cast<float>(static_cast<double>(depth)*(1.0/127.0));
        switch(destination)
        {
            case LfoDestination::Oscillator2:case LfoDestination::BothOscillators:
            {
                const float positive=amount*5.0f,negative=amount*-5.0f;
                return {static_cast<int>(negative*128.0f),static_cast<int>(positive*128.0f)};
            }
            case LfoDestination::Filter:return {0,static_cast<int>(amount*1024.0f)};
            case LfoDestination::Amplitude:return {static_cast<int>(amount*2048.0f),0};
            case LfoDestination::PulseWidth:return {static_cast<int>(amount*-128.0f),static_cast<int>(amount*128.0f)};
            case LfoDestination::Fm:return {0,static_cast<int>(amount*512.0f)};
            case LfoDestination::Pan:return {static_cast<int>(amount*-64.0f),static_cast<int>(amount*64.0f)};
            default:return {0,0};
        }
    }

}
