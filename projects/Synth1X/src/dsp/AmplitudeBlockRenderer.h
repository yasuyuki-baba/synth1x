#pragma once
#include "ReceiverControls.h"
#include "AmplitudeEnvelope.h"

namespace synth1x::dsp
{
    // Horizons and the optional known event end are relative to this call.
    // repeatHorizon continues the caller's gain grid after its initial span.
    struct AmplitudeBlockSettings { int gainOffset {};int gainHorizon {};int repeatHorizon {};int boundaryFrames {}; };

    struct AmplitudeBlockRenderer
    {
        struct Result
        {
            bool signalDispatched {};
            bool advanceEffectClock {};
        };

        // All inputs use physical VCA attenuation coordinates. Keep independent
        // slot products, caller-owned alignment and each source's held spans.
        static Result render(const AmplitudeBlockSettings& config,const ReceiverLfoView& lfos,
            AmplitudeEnvelope& amplitude,int frames,float* gains)
        {
            Result result;
            for(int offset=0;offset<frames;)
            {
                std::array<float,2> control{};std::array<bool,2> clockMute{};int count=frames-offset;
                int horizon=config.gainHorizon;
                if(config.repeatHorizon>0)
                {
                    horizon=offset<config.gainHorizon ? config.gainHorizon-offset
                        : config.repeatHorizon-(offset-config.gainHorizon)%config.repeatHorizon;
                    if(config.boundaryFrames>offset)horizon=std::min(horizon,config.boundaryFrames-offset);
                    count=std::min(count,horizon);
                }
                for(size_t slot=0;slot<2;++slot)
                {
                    if(lfos.available && lfos.receivers[slot].enabled && lfos.receivers[slot].destination==LfoDestination::Amplitude)
                    {
                        clockMute[slot]=lfos.sineAmplitudeMute[slot] && lfos.sineAmplitudeMute[slot][offset];
                        control[slot]=lfos.values[slot][offset];
                        count=std::min(count,lfos.remaining[slot][offset]);
                        if(horizon>0)horizon=std::min(horizon,lfos.remaining[slot][offset]);
                    }
                }
                amplitude.setModulation(control[0],control[1],clockMute);
                amplitude.render(gains+config.gainOffset+offset,count,horizon);
                result.signalDispatched = result.signalDispatched || !amplitude.wasSilent();
                result.advanceEffectClock = result.advanceEffectClock || amplitude.shouldAdvanceEffectClock();
                offset+=count;
            }
            return result;
        }
    };
}
