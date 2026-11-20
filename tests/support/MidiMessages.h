#pragma once
#include "audio/Midi.h"

namespace synth1x::test
{
inline audio::MidiMessage pitchWheel(int channel, int value)
{
    return {0xe0 + channel - 1, value & 127, (value >> 7) & 127};
}
}
