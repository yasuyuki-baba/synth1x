#pragma once
#include "Oscillator.h"
#include "OscillatorStack.h"
#include "SubOscillatorStack.h"
#include "OscillatorSync.h"
#include "FmPhaseCorrection.h"
#include "audio/AudioTypes.h"
#include <array>

namespace synth1x::dsp {
    class OscillatorSection {
    public:
        void prepare(const ::synth1x::audio::ProcessSpec& spec) {
            sampleRate = spec.sampleRate;
            osc1.setSampleRate(sampleRate);
            osc2.setSampleRate(sampleRate);
        }
        void reset() { osc1.reset(); osc2.reset(); stackPhases.fill(0); subSharedPhase=0; fmPhase.reset(); }
        void resetFmPhase() noexcept { fmPhase.reset(); }
        void resetPhase(float phase01) {
            fmPhase.reset();
            osc1.resetPhase(phase01, params.osc1Wave);
            osc2.resetPhase(phase01, params.osc2Wave);
        }
        uint32_t getOsc1Phase() const noexcept { return osc1.getPhaseFixed27(); }
        std::array<uint32_t,4> readPhaseWords() const noexcept {
            return {{osc1.getPhaseFixed27(),osc2.getPhaseFixed27(),
                     osc1.getPhaseIncFixed27(),osc2.getPhaseIncFixed27()}};
        }

        float resolvePulseWidth(float width,float control) noexcept
        {
            // Only value conversion is cached. Phase, receivers and random state
            // still advance on their original sample/segment boundaries.
            if(!pulseWidthReady || width!=pulseWidthSource || control!=pulseWidthControl)
            {
                pulseWidthBins=ModulationEnvelope::pulseWidthForDial(fractionalDial(width),control);
                pulseWidthSource=width;pulseWidthControl=control;pulseWidthReady=true;
            }
            return pulseWidthBins;
        }

        struct Input
        {
            float firstFrequency {},secondFrequency {};
            float pulseWidth {0.5f},mix {},fmDepth {};
            bool sync {},ring {};
            float subGain {};
            int subShape {},subOctave {-1};
            const WaveTableBank* bank {};
            float pulseWidthBins {-1},firstPitch {},secondPitch {};
            bool pitchReceiver {},correctFmPhase {},fmTargetActive {};
        };

        float processSample(const Input& input)
        {
            float output;
            renderFrame<false>(&output,1,input);
            return output;
        }

        void processFrame(float* output,int factor,const Input& input)
        {
            renderFrame<true>(output,factor,input);
        }

    private:
        template<bool oversampled>
        void renderFrame(float* output,int factor,const Input& input)
        {
            const int sampleCount=oversampled ? factor : 1;
            float f1=input.firstFrequency;
            float f2=input.secondFrequency;
            const float base1=f1;
            const float pulseBins=input.pulseWidthBins>=0 ? input.pulseWidthBins
                : resolvePulseWidth(input.pulseWidth,0.0f);
            if(input.pitchReceiver)
            {
                f1=ModulationEnvelope::pitchFrequency(f1,input.firstPitch);
                f2=ModulationEnvelope::pitchFrequency(f2,input.secondPitch);
            }
            // The fixed VCO ceiling also applies without an active pitch receiver.
            // Limit after modulation, before table selection and phase advancement.
            f1=std::min(f1,22000.0f);
            f2=std::min(f2,22000.0f);
            const bool nativeStack=std::lround(stackDial)>0 && input.bank != nullptr;
            const uint32_t firstStart = osc1.getPhaseFixed27();
            osc1.updatePhaseIncrement(f1);
            osc2.updatePhaseIncrement(f2);
            const auto secondRead=osc2.prepareRead(params.osc2Wave,input.pulseWidth,input.bank,pulseBins);
            const bool ringUsesFmGain=input.ring && (input.fmDepth!=0 || input.fmTargetActive);
            const auto readSecond=[&](uint32_t phase) {
                float value=secondRead.at(phase);
                if(params.osc2Wave==4 && !ringUsesFmGain)value*=.65f;
                if(params.osc2Wave==1 && ringUsesFmGain)value*=2.0f;
                return value;
            };
            const uint32_t secondStart=osc2.getPhaseFixed27();
            const float fmSource=readSecond(secondStart);

            // Native isolated two-tone/noise captures use a linear crossfade.
            const float vol1 = 1.0f - input.mix;
            const float vol2 = input.mix;
            const float scale = SubOscillatorStack::normalizer(input.subGain,input.mix);

            // Native Windows kernels use MULSS then CVTTSS2SI, not nearest rounding.
            // VCO dispatch tests Ring (+0x168) before its FM branch. The
            // audited ring kernel advances both phases without an FM offset.
            const auto fmAdvance=fmPhase.advance(input.ring ? 0.0f : input.fmDepth,fmSource,
                osc1.getPhaseIncFixed27(),firstStart,input.correctFmPhase && !input.ring);
            const uint32_t phaseAdd=fmAdvance.audibleOffset;
            const int64_t firstAdvance=int64_t(osc1.getPhaseIncFixed27())+static_cast<int32_t>(phaseAdd);
            const auto firstRead=osc1.prepareRead(params.osc1Wave,input.pulseWidth,input.bank,pulseBins);
            const bool down=input.subOctave<0;
            const float* subTable=nullptr;
            if(input.subGain>0 && input.bank)
            {
                const float frequency=f1*(down ? 0.5f : 1.0f);
                const bool nativeSub=!nativeStack || (input.subShape>=0 && input.subShape<=2);
                subTable=nativeSub ? input.bank->getNativeTableFor(input.subShape,frequency).first
                                   : input.bank->getTableFor(input.subShape,frequency).first;
            }
            const float* stackTable=nullptr;
            if(nativeStack)
            {
                if(stackBase != base1 || cachedStackDial != stackDial)
                {
                    stackFrequencies=OscillatorStack::frequencies(base1,stackDial);
                    stackBase=base1;cachedStackDial=stackDial;
                }
                const int shape=params.osc1Wave==1 ? 5 : params.osc1Wave;
                stackTable=(shape==0 || shape==2 || shape==5)
                    ? input.bank->getNativeTableFor(shape,f1).first : input.bank->getTableFor(shape,f1).first;
            }

            // FM, sync and noise advance once. Fine reads share the resolved
            // tables and follow the signed advance up to the next host boundary.
            std::array<int64_t,8> laneAdvance;
            if(stackTable)
                for(size_t lane=0;lane<stackPhases.size();++lane)
                {
                    const float frequency=input.firstPitch!=0
                        ? ModulationEnvelope::pitchFrequency(stackFrequencies[lane],input.firstPitch)
                        : std::min(stackFrequencies[lane],22000.0f);
                    laneAdvance[lane]=int64_t(compatiblePhaseIncrement(frequency,sampleRate))+static_cast<int32_t>(phaseAdd);
                }
            for(int step=0;step<sampleCount;++step)
            {
                const auto offset=[&](int64_t delta) {
                    return step==0 ? 0u : static_cast<uint32_t>(delta*step/sampleCount);
                };
                const uint32_t firstOffset=offset(firstAdvance);
                const uint32_t firstPhase=(firstStart+firstOffset)&kPhaseMask27;
                const uint32_t subPhase=subSharedPhase+firstOffset;
                const float s2=step==0 ? fmSource : readSecond(secondStart+offset(osc2.getPhaseIncFixed27()));
                float s1;
                float sub=0.0f;
                if(stackTable)
                {
                    const auto read=[&](uint32_t phase) {
                        const uint32_t index=(phase>>16)&2047;
                        const float value=stackTable[index];
                        if(params.osc1Wave!=1)return value;
                        return PulseWidthRead::interpolate(pulseBins,[&](uint32_t width) {
                            return value-stackTable[(index+width)&2047];
                        });
                    };
                    const auto readSub=[&](uint32_t phase) {
                        return subTable[SubOscillatorStack::tableIndex(phase,down)];
                    };
                    const uint32_t lane0=stackPhases[0]+offset(laneAdvance[0]);
                    s1=read(lane0)+read(firstPhase);
                    if(subTable)sub=down ? readSub(subPhase)+readSub(lane0) : readSub(lane0)+readSub(subPhase);
                    for(size_t lane=1;lane<stackPhases.size();++lane)
                    {
                        const uint32_t phase=stackPhases[lane]+offset(laneAdvance[lane]);
                        s1+=read(phase);
                        if(subTable)sub+=readSub(phase);
                    }
                    s1*=params.osc1Wave==1 ? 0.15f : 0.3f;
                    if(!input.ring)sub*=0.3f;
                }
                else s1=firstRead.at(firstPhase);
                float combined=input.ring ? ((s2*(vol2*scale))*s1+s1*(vol1*scale))
                                          : (s1*vol1+s2*vol2)*scale;
                if(input.subGain>0 && nativeStack)
                    combined+=(sub*vol1*input.subGain)*scale;
                else if(subTable)
                {
                    const uint32_t phase=down ? subPhase>>1 : subPhase;
                    const uint32_t index=(phase>>16)&2047u;
                    float value=subTable[(index+1)&2047u]-subTable[index];
                    value*=static_cast<float>(phase&65535u);value*=1.0f/65536.0f;value+=subTable[index];
                    combined+=(value*(vol1*scale))*input.subGain;
                }
                output[step]=combined;
            }
            if(stackTable)
                for(size_t lane=0;lane<stackPhases.size();++lane)
                    stackPhases[lane]+=static_cast<uint32_t>(laneAdvance[lane]);

            subSharedPhase+=osc1.getPhaseIncFixed27()+phaseAdd;
            osc1.advancePhaseFixedWithOffset(phaseAdd);
            if (params.osc2Wave == 4) {
                if(input.sync)osc2.synchronizeNoise(fmAdvance.referenceBefore,fmAdvance.referenceAfter);
            } else if (input.sync) {
                const auto next=OscillatorSync::advance(fmAdvance.referenceBefore,osc2.getPhaseFixed27(),
                    osc1.getPhaseIncFixed27(),osc2.getPhaseIncFixed27(),fmAdvance.referenceOffset);
                osc2.setPhaseFixed27(next.second);
            } else {
                osc2.advancePhase();
            }
        }

    public:
        struct Settings {
            int osc1Wave{0}, osc2Wave{0};
        } params;
        uint32_t subSharedPhase {0};
        float stackDial {0}, cachedStackDial {-1};
        float stackBase {-1.0f};
        std::array<float,8> stackFrequencies {};
        std::array<uint32_t,8> stackPhases {};
        float sampleRate{44100.0f};
        Oscillator osc1, osc2;
        FmPhaseCorrection fmPhase;
    private:
        float pulseWidthSource{},pulseWidthControl{},pulseWidthBins{};
        bool pulseWidthReady{};
    };
}
