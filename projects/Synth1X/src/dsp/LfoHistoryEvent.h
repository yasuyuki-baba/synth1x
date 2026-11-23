#pragma once
#include <array>

namespace synth1x::dsp
{
    struct LfoHistoryEvent
    {
        enum class Kind { Rate, Load, LoadWithMidiRouting, TempoSync, RandomEdit };

        Kind kind {Kind::Rate};
        // Slot and value for rate/sync, parameter and value for a random edit,
        // or the two LFO rates for a load.
        int a{},b{};
        std::array<int,4> routes{};
        std::array<int,2> sync{};
        // Native destination/wave/depth, captured with each load marker.
        std::array<std::array<int,3>,2> randomControls{};
        bool randomAdmitted=false;
        bool randomArpeggio=false;
    };

}
