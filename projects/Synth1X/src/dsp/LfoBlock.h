#pragma once
#include <array>

namespace synth1x::dsp
{
    // Engine-owned, prepared storage. Voices only read it during rendering.
    struct LfoBlock
    {
        int frameCapacity {256};
        std::array<std::array<bool,256>,2> sineAmplitudeMute {};
        int startSample = 0;
        std::array<std::array<float,256>,2> controls {};
        std::array<std::array<int,256>,2> controlRemaining {};
    };

}
