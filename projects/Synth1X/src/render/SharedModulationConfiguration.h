#pragma once
#include "../RenderControls.h"
#include "RenderHistory.h"
#include "../dsp/SharedModulationSettings.h"

namespace synth1x::render
{
    inline dsp::SharedModulationSettings sharedModulation(const RenderControls& p,const RenderHistory& history={})
    {
        return {{{p.lfo1,p.lfo2}},p.hostBpm,
            p.hasPitchLfo(),p.hasLfo(LfoDestination::Filter),p.hasLfo(LfoDestination::PulseWidth),
            history.nativeMidiRoutingLoadHistory,history.nativeMidiRoutingControls};
    }
}
