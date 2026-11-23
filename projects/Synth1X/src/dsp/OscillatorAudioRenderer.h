#pragma once
#include "OscillatorSection.h"
#include "ReceiverControls.h"

namespace synth1x::dsp
{
    struct OscillatorAudioSettings
    {
        bool secondTracksKeyboard {},pitchReceiver {},pitchEnvelope {},pulseReceiver {},pulseEnvelope {};
        float pulseWidth {};
        int subWave {},subOctave {-1};
        bool correctFmPhase {};
    };
    struct OscillatorAudioInput
    {
        float firstFrequency {},secondFrequency {},pulseWidth {},mix {},subGain {};
        bool sync {},ring {};
        float firstPitch {},secondPitch {},glidePitch {},envelope {};
        FmControl fm {};
        float pulseControl {};
    };
    struct OscillatorAudioRenderer
    {
        static float process(OscillatorSection& oscillator,const OscillatorAudioSettings& config,
            const OscillatorAudioInput& input,const ReceiverLfoView& lfos,int position,
            const synth1x::WaveTableBank* bank,float* frame=nullptr,int factor=1)
        {
            float firstFrequency=input.firstFrequency,secondFrequency=input.secondFrequency;
            float firstPitch=input.firstPitch+input.glidePitch;
            float secondPitch=input.secondPitch+(config.secondTracksKeyboard ? input.glidePitch : 0);
            if(config.pitchEnvelope)
            {
                const float control=input.envelope;
                if(config.pitchReceiver)
                {
                    secondPitch+=control;
                }
                else
                {
                    secondFrequency=ModulationEnvelope::pitchFrequency(secondFrequency,control);
                }
            }
            float pulse=input.pulseControl;
            if(config.pulseReceiver && lfos.available)
                for(size_t slot=0;slot<2;++slot)
                    if(lfos.receivers[slot].enabled && lfos.receivers[slot].destination==LfoDestination::PulseWidth)
                        pulse+=lfos.values[slot][position];
            if(config.pulseEnvelope)pulse+=input.envelope;
            const float width=config.pulseReceiver || config.pulseEnvelope
                ? config.pulseWidth : input.pulseWidth;
            const OscillatorSection::Input oscillatorInput {
                .firstFrequency=firstFrequency, .secondFrequency=secondFrequency,
                .pulseWidth=input.pulseWidth, .mix=input.mix, .fmDepth=input.fm.depth,
                .sync=input.sync, .ring=input.ring, .subGain=input.subGain,
                .subShape=config.subWave, .subOctave=config.subOctave, .bank=bank,
                .pulseWidthBins=oscillator.resolvePulseWidth(width,pulse),
                .firstPitch=firstPitch, .secondPitch=secondPitch,
                .pitchReceiver=config.pitchReceiver, .correctFmPhase=config.correctFmPhase,
                .fmTargetActive=input.fm.targetActive
            };
            if(frame)
            {
                oscillator.processFrame(frame,factor,oscillatorInput);
                return frame[0];
            }
            return oscillator.processSample(oscillatorInput);
        }
    };
}
