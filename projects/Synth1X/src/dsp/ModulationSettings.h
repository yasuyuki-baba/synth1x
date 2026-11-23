#pragma once

#include <array>

// Value-only controls shared by the loading bridge and modulation components.
// No file format, host automation, voice allocation or global engine dependency.
namespace synth1x
{
    enum class LfoDestination
    {
        Oscillator2 = 0,
        BothOscillators = 1,
        Filter = 2,
        Amplitude = 3,
        PulseWidth = 4,
        Fm = 5,
        Pan = 6
    };

    struct LfoSettings
    {
        bool on {false};
        int wave {0}; // Sine, triangle, saw, square, sample-and-hold, smoothed random.
        LfoDestination dest {LfoDestination::Oscillator2};
        float speed {0.5f},amount {0.0f};
        bool tempoSync {false},keySync {false};
    };

    struct ModulationEnvelopeControls
    {
        float amountDial {64};
        float attackDial {0},decayDial {0};
        bool  on         { false };
        int   dest       { 0 };     // 0=osc2, 2=fm, 3=pw
    };

}
