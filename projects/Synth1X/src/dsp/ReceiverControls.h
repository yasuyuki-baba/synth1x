#pragma once
#include "ModulationSettings.h"
#include <algorithm>
#include <array>

namespace synth1x::dsp
{
    // Signal amplitudes and phase-unit values must retain fractional data.
    using ControlSignal=float;
    using VoiceControlBuffer=std::array<ControlSignal,256>;
    using VoiceSpanBuffer=std::array<int,256>;

    struct FmControl { float depth; bool targetActive; };
    using FmControlBuffer=std::array<FmControl,256>;

    struct ReceiverLfoView
    {
        struct Receiver { bool enabled {};LfoDestination destination {LfoDestination::Oscillator2}; };
        std::array<Receiver,2> receivers {};
        std::array<const ControlSignal*,2> values {};
        std::array<const int*,2> remaining {};
        bool available {};
        std::array<const bool*,2> sineAmplitudeMute {};
        static bool reachesOscillator(const Receiver& receiver) noexcept
        {
            return receiver.enabled && (receiver.destination==LfoDestination::Oscillator2 || receiver.destination==LfoDestination::BothOscillators
                || receiver.destination==LfoDestination::PulseWidth || receiver.destination==LfoDestination::Fm);
        }
        int oscillatorFrames(int position,int frames) const noexcept
        {
            if(available)for(size_t slot=0;slot<2;++slot)
                if(reachesOscillator(receivers[slot]))frames=std::min(frames,remaining[slot][position]);
            return frames;
        }
    };

}
