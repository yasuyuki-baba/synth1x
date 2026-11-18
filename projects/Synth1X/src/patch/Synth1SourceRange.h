#pragma once

namespace synth1x
{
    // Disabled routes are patch values outside the historical host range.
    constexpr bool isDisabledMidiDestination(int index,double value) noexcept
    { return (index==87 || index==89) && value==-1; }
    // Serialized MIDI source words carry status/controller bytes, unlike knobs.
    constexpr int sourceMinimum(int index) noexcept { return index == 9 ? -24 : (index == 86 || index == 88) ? -1 : 0; }
    constexpr int sourceMaximum(int index) noexcept
    {
        return index == 9 ? 24 : (index == 86 || index == 88) ? 65535 : 127;
    }
}
