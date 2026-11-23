#pragma once
#include "../RenderControls.h"
#include "LfoViews.h"

namespace synth1x::render
{
    inline bool outputPanLfo(const RenderControls& p) noexcept
    {
        return p.hasLfo(LfoDestination::Pan);
    }
    inline dsp::ReceiverLfoView outputPanControls(const RenderControls& p,const dsp::LfoBlock* block,int start)
    {
        dsp::ReceiverLfoView view;
        if(block==nullptr)return view;
        const int offset=start-block->startSample;
        const std::array<const LfoSettings*,2> lfos{&p.lfo1,&p.lfo2};
        for(size_t slot=0;slot<2;++slot)
        {
            const auto& lfo=*lfos[slot];
            view.receivers[slot]={lfo.on && lfo.dest==LfoDestination::Pan,LfoDestination::Pan};
            view.values[slot]=block->controls[slot].data()+offset;
            view.remaining[slot]=block->controlRemaining[slot].data()+offset;
        }
        view.available=true;return view;
    }
}
