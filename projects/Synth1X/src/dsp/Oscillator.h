#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "FractionalDial.h"
#include "NoiseGenerator.h"
#include "ModulationEnvelope.h"
#include "PulseWidthRead.h"
#include "WaveTableBank.h"

namespace synth1x::dsp {

    static constexpr uint32_t kPhaseBits27 = 27;
    static constexpr uint32_t kPhaseMask27 = 0x07ffffff;
    static constexpr uint32_t kPhaseScale27 = (1u << kPhaseBits27);

    // Native frequency and phase words are discrete compatibility boundaries.
    // Audio samples and interpolation use Float32 throughout the graph.
    inline uint32_t compatiblePhaseIncrement(float frequency,float sampleRate) noexcept
    {
        const float increment=(frequency/sampleRate)*static_cast<float>(kPhaseScale27);
        return static_cast<uint32_t>(static_cast<float>(increment))&kPhaseMask27;
    }

    class Oscillator {
    public:
        Oscillator() { reset(); }
        void setSampleRate(float sr) {
            if(sampleRate!=sr)incrementReady=false;
            sampleRate=sr;
        }

        uint32_t getPhaseFixed27() const noexcept { return phaseAcc32; }
        void setPhaseFixed27(uint32_t p) noexcept { phaseAcc32 = p & kPhaseMask27; }
        uint32_t getPhaseIncFixed27() const noexcept { return phaseIncFixed27; }
        void synchronizeNoise(uint32_t master,uint32_t nextMaster) noexcept { noise.synchronize(master,nextMaster); }

        float renderSample(int waveIndex,float pulseWidth,uint32_t phaseOffset27=0,
            const WaveTableBank* bank=nullptr,float nativePulseWidthBins=-1) noexcept
        {return readSample(waveIndex,pulseWidth,phaseOffset27,bank,nativePulseWidthBins);}

        float readSample(int waveIndex,float pulseWidth,uint32_t phaseOffset27=0,
            const WaveTableBank* bank=nullptr,float nativePulseWidthBins=-1) noexcept
        {
            return prepareRead(waveIndex,pulseWidth,bank,nativePulseWidthBins)
                .at(phaseAcc32+phaseOffset27);
        }

        // The waveform and pulse width stay fixed throughout a host frame.
        struct WaveRead {
            const float* table {};
            float held {};
            uint32_t pulseOffset {};
            float pulseFraction {};

            float at(uint32_t phase) const noexcept {
                if(!table)return held;
                const float origin=readTable(table,phase);
                if(!pulseOffset)return origin;
                const float first=.5f*(origin-readTable(table,phase+pulseOffset));
                const float second=.5f*(origin-readTable(table,phase+pulseOffset+(1u<<16)));
                return interpolate(first,second,pulseFraction);
            }
        };

        WaveRead prepareRead(int waveIndex,float pulseWidth,const WaveTableBank* bank,
                             float nativePulseWidthBins) noexcept
        {
            if(waveIndex==4)return {nullptr,noise.sample()};
            if(!bank)return {};
            if(waveIndex==1 && bank->hasShape(5))
            {
                const float bins=std::clamp(nativePulseWidthBins>=0 ? nativePulseWidthBins
                    : ModulationEnvelope::pulseWidthForDial(fractionalDial(pulseWidth),0.0f),10.0f,2037.0f);
                const auto low=static_cast<uint32_t>(bins);
                return {bank->getNativeTableFor(5,frequencyHz).first,0.0f,low<<16,bins-low};
            }
            const float frequency=phaseIncFixed27*sampleRate/kPhaseScale27;
            return {waveIndex==0 || waveIndex==2 ? bank->getNativeTableFor(waveIndex,frequencyHz).first
                                                : bank->getTableFor(waveIndex,frequency).first};
        }

        void advancePhase() noexcept {
            uint32_t next = phaseAcc32 + phaseIncFixed27;
            phaseAcc32 = next & kPhaseMask27;
        }

        void advancePhaseFixedWithOffset(uint32_t offset) noexcept {
            uint32_t next = (phaseAcc32 + phaseIncFixed27 + offset);
            phaseAcc32 = next & kPhaseMask27;
        }

        void updatePhaseIncrement(float freqHz) noexcept {
            if(incrementReady && frequencyHz==freqHz)return;
            frequencyHz = freqHz;
            phaseIncFixed27=compatiblePhaseIncrement(freqHz,sampleRate);
            if (phaseIncFixed27 == 0) phaseIncFixed27 = 1;
            incrementReady=true;
        }

        void reset() noexcept { phaseAcc32 = 0; noise.reset(); }
        void resetPhase(float phase01, int waveIndex) noexcept {
            const float v = (float)phase01 * (float)kPhaseScale27;
            phaseAcc32 = (uint32_t)std::floor(v) & kPhaseMask27;
        }

    private:
        static float interpolate(float first,float second,float fraction) noexcept
        {return first+(second-first)*fraction;}
        static float readTable(const float* data,uint32_t phase) noexcept
        {
            const uint32_t index=(phase>>16)&2047u;
            return interpolate(data[index],data[(index+1)&2047u],
                static_cast<float>(phase&65535u)*(1.0f/65536.0f));
        }
        uint32_t phaseAcc32{0}, phaseIncFixed27{1};
        NoiseGenerator noise;
        float sampleRate{44100.0f};
        float frequencyHz{440.0f};
        bool incrementReady{};
    };

}
