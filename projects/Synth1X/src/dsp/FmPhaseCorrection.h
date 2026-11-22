#pragma once
#include <cstdint>
#include <cmath>

namespace synth1x::dsp
{
    // Common FM product correction; no coefficient filter or waveform policy.
    // Fractional audible increments accumulate independently of the Sync clock.
    class FmPhaseCorrection
    {
    public:
        struct Advance
        {
            uint32_t audibleOffset {},referenceOffset {},referenceBefore {},referenceAfter {};
        };
        void reset() noexcept { ready=false; }
        Advance advance(float depth,float source,uint32_t increment,uint32_t phase,bool enabled) noexcept
        {
            constexpr uint32_t mask=0x07ffffffu;
            // Only the compatible Sync/noise reference retains the float product.
            const int32_t native=static_cast<int32_t>(static_cast<float>(depth)*static_cast<float>(source));
            if(!enabled)
            {
                ready=false;
                const auto offset=static_cast<uint32_t>(native);
                return {offset,offset,phase,(phase+increment+offset)&mask};
            }
            if(!ready || phase!=lastPhase)
            {
                ready=true;reference=phase;phaseCarry=0;
            }
            const float lower=std::floor(depth);
            const double fraction=depth-lower;
            double incrementWithCarry=native+phaseCarry;
            if(fraction!=0)
            {
                const auto first=static_cast<int32_t>(lower*source);
                const auto second=static_cast<int32_t>((lower+1.0f)*source);
                incrementWithCarry=first+(second-first)*fraction+phaseCarry;
            }
            const auto audible=static_cast<int32_t>(incrementWithCarry);
            phaseCarry=incrementWithCarry-audible;
            const uint32_t before=reference;
            // Sync/slave/noise follows the compatible online reference, separately
            // from the corrected audible oscillator phase. No recorded input.
            reference=(reference+increment+static_cast<uint32_t>(native))&mask;
            lastPhase=(phase+increment+static_cast<uint32_t>(audible))&mask;
            return {static_cast<uint32_t>(audible),static_cast<uint32_t>(native),before,reference};
        }
    private:
        bool ready {};
        uint32_t reference {},lastPhase {};
        double phaseCarry {};
    };
}
