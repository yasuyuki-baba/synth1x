#include <gtest/gtest.h>
#include "dsp/FmPhaseCorrection.h"
#include "dsp/FractionalDial.h"
#include "dsp/FrequencyModulation.h"
#include "dsp/NoiseGenerator.h"
#include "render/VoiceAudioConfiguration.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {
constexpr uint32_t mask=0x07ffffffu;
int32_t offset(uint32_t word) {return static_cast<int32_t>(word);}
}

TEST(FmPhaseCorrection, DisabledPathPreservesCompatiblePhaseAdvancement)
{
    synth1x::dsp::FmPhaseCorrection correction;
    uint32_t phase=12345;
    for(int i=0;i<1000;++i)
    {
        const float depth=123456.75f,source=i%3==0 ? -.65f : .65f;
        const auto expected=static_cast<uint32_t>(static_cast<int32_t>(depth*source));
        const auto result=correction.advance(depth,source,400000,phase,false);
        EXPECT_EQ(result.audibleOffset,expected);EXPECT_EQ(result.referenceOffset,expected);
        EXPECT_EQ(result.referenceBefore,phase);
        phase=(phase+400000+expected)&mask;EXPECT_EQ(result.referenceAfter,phase);
    }
}

TEST(FmPhaseCorrection, ConstantCoefficientTracksItsInterpolatedPhase)
{
    synth1x::dsp::FmPhaseCorrection correction;
    uint32_t phase=0;int64_t difference=0;
    constexpr float depth=123456.75f,source=.3f;
    const int32_t native=static_cast<int32_t>(depth*source);
    // Adjacent compatible products for 123456 and 123457 are 37036/37037.
    const double interpolated=37036.75;
    for(int i=0;i<20000;++i)
    {
        const auto result=correction.advance(depth,source,400000,phase,true);
        difference+=offset(result.audibleOffset)-offset(result.referenceOffset);
        EXPECT_NEAR(difference,(i+1)*(interpolated-native),1.0);
        phase=(phase+400000+result.audibleOffset)&mask;
    }
}

TEST(FmPhaseCorrection, ReferenceClockPreservesSynchronizedNoiseHistory)
{
    synth1x::dsp::FmPhaseCorrection correction;
    synth1x::dsp::NoiseGenerator nativeNoise,correctedNoise;
    uint32_t nativePhase=0,phase=0;bool differs=false;
    for(int i=0;i<20000;++i)
    {
        const float nativeSource=nativeNoise.sample()*.65f;
        const float source=correctedNoise.sample()*.65f;
        ASSERT_EQ(source,nativeSource);
        const float depth=static_cast<float>((i/64)%16)*100000+.25f;
        const auto result=correction.advance(depth,source,400000,phase,true);
        const uint32_t next=(nativePhase+400000+static_cast<uint32_t>(static_cast<int32_t>(depth*nativeSource)))&mask;
        EXPECT_EQ(result.referenceBefore,nativePhase);EXPECT_EQ(result.referenceAfter,next);
        nativeNoise.synchronize(nativePhase,next);
        correctedNoise.synchronize(result.referenceBefore,result.referenceAfter);
        EXPECT_EQ(nativeNoise.state,correctedNoise.state);
        nativePhase=next;phase=(phase+400000+result.audibleOffset)&mask;
        differs|=phase!=nativePhase;
    }
    EXPECT_TRUE(differs);
}

TEST(FmPhaseCorrection, OscillatorUsesReferencePhaseForNoiseSync)
{
    synth1x::dsp::OscillatorSection native,corrected;
    for(auto* oscillator:{&native,&corrected})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=4;
    }
    bool phaseDiffers=false;
    for(int frame=0;frame<20000;++frame)
    {
        const float depth=static_cast<float>((frame/64)%16)*100000+.25f;
        const auto render=[&](auto& oscillator,bool correcting) {
            return oscillator.processSample({
                .firstFrequency=261.625f, .secondFrequency=261.625f, .mix=1, .fmDepth=static_cast<float>(depth),
                .sync=true, .subOctave=0, .correctFmPhase=correcting
            });
        };
        // With no wavetable and full OSC2 mix, output is the synchronized noise.
        EXPECT_EQ(render(native,false),render(corrected,true));
        phaseDiffers|=native.getOsc1Phase()!=corrected.getOsc1Phase();
    }
    EXPECT_TRUE(phaseDiffers);
}

TEST(FmPhaseCorrection, CoefficientStepsTurnsAndJumpsRemainUnfiltered)
{
    // Ramps, triangle turns, square jumps and held irregular steps all enter
    // the same phase boundary. Integral products must keep every native knot.
    constexpr std::array<float,16> depths {0,100000,200000,300000,200000,100000,0,0,
        600000,600000,0,0,120000,540000,30000,210000};
    for(float source:{-.5f,.5f})
    {
        synth1x::dsp::FmPhaseCorrection correction;uint32_t phase=0,reference=0;
        for(int frame=0;frame<8192;++frame)
        {
            const float depth=depths[static_cast<size_t>(frame/64)%depths.size()];
            const uint32_t expected=static_cast<uint32_t>(static_cast<int32_t>(depth*source));
            const auto result=correction.advance(depth,source,400000,phase,true);
            EXPECT_EQ(result.audibleOffset,expected);
            EXPECT_EQ(result.referenceBefore,reference);
            reference=(reference+400000+expected)&mask;
            EXPECT_EQ(result.referenceAfter,reference);
            phase=(phase+400000+result.audibleOffset)&mask;
            EXPECT_EQ(phase,reference);
        }
    }
}

TEST(FmPhaseCorrection, QuarterStepsAccumulateUntilModulationStops)
{
    for(float sign:{-1.f,1.f})
    {
        synth1x::dsp::FmPhaseCorrection correction;uint32_t phase=0;
        int64_t difference=0;bool fractionalProductRetained=false;
        for(int frame=0;frame<8192;++frame)
        {
            const float depth=frame<4096 ? sign*.25f : 0.f;
            const auto result=correction.advance(depth,1,400000,phase,true);
            difference+=offset(result.audibleOffset)-offset(result.referenceOffset);
            EXPECT_EQ(difference,static_cast<int64_t>(sign*(std::min(frame+1,4096)/4)));
            fractionalProductRetained|=result.audibleOffset!=result.referenceOffset;
            phase=(phase+400000+result.audibleOffset)&mask;
        }
        EXPECT_TRUE(fractionalProductRetained);
        EXPECT_EQ(difference,static_cast<int64_t>(sign*1024));
    }
}

TEST(FmPhaseCorrection, ControlChangesRetainBothPhaseHistories)
{
    synth1x::dsp::FmPhaseCorrection correction;uint32_t phase=0;
    auto step=[&](float depth) {
        const auto result=correction.advance(depth,1,400000,phase,true);
        phase=(phase+400000+result.audibleOffset)&mask;return result;
    };
    auto previous=step(.25f);
    for(int i=0;i<64;++i)previous=step(.25f);
    ASSERT_NE(phase,previous.referenceAfter);
    const auto changed=step(100000.25f);
    EXPECT_EQ(changed.referenceBefore,previous.referenceAfter);
    EXPECT_EQ(changed.referenceAfter,(previous.referenceAfter+500000)&mask);
    EXPECT_NE(phase,changed.referenceAfter);
    // The new whole increment preserves the phase difference already accumulated.
    for(int i=0;i<2048;++i)previous=step(100000);
    const int32_t distance=static_cast<int32_t>((phase-previous.referenceAfter+0x04000000u)&mask)-0x04000000;
    EXPECT_EQ(distance,16);
}

TEST(FmPhaseCorrection, ResetExternalPhaseAndDisableDiscardStaleCorrection)
{
    synth1x::dsp::FmPhaseCorrection correction;uint32_t phase=0;
    auto step=[&](float depth,bool enabled=true) {
        const auto result=correction.advance(depth,1,400000,phase,enabled);
        phase=(phase+400000+result.audibleOffset)&mask;return result;
    };
    step(.75f);
    phase=1234;auto result=step(.5f);
    EXPECT_EQ(result.referenceBefore,1234u);EXPECT_EQ(result.audibleOffset,0u);
    correction.reset();const auto resetPhase=phase;result=step(.75f);
    EXPECT_EQ(result.referenceBefore,resetPhase);EXPECT_EQ(result.audibleOffset,0u);
    const auto disabledPhase=phase;result=step(0,false);
    EXPECT_EQ(result.referenceBefore,disabledPhase);EXPECT_EQ(result.audibleOffset,0u);
    const auto enabledPhase=phase;result=step(.5f);
    EXPECT_EQ(result.referenceBefore,enabledPhase);EXPECT_EQ(result.audibleOffset,0u);
}

TEST(FmPhaseCorrection, PhaseCorrectionIsEnabledAcrossWaveformsAndEnvelopeDestinations)
{
    synth1x::RenderControls settings;
    settings.lfo1.on=settings.lfo2.on=true;
    settings.lfo1.dest=settings.lfo2.dest=synth1x::LfoDestination::Fm;
    for(int first=0;first<6;++first)for(int second=0;second<6;++second)
        for(bool envelope:{false,true})
        {
            settings.lfo1.wave=first;settings.lfo2.wave=second;
            settings.modEnv.on=envelope;
            const auto config=synth1x::render::oscillatorAudio(settings,false);
            EXPECT_TRUE(config.correctFmPhase);
            EXPECT_FALSE(synth1x::render::oscillatorAudio(settings,true).correctFmPhase);
        }
    settings.lfo1.on=settings.lfo2.on=false;
    for(int destination:{0,2,3})
    {
        settings.modEnv.on=true;settings.modEnv.dest=destination;
        EXPECT_TRUE(synth1x::render::oscillatorAudio(settings,false).correctFmPhase);
    }
}

TEST(FmPhaseCorrection, NativeRingDispatchBypassesFmCorrection)
{
    synth1x::dsp::OscillatorSection native,corrected;
    for(auto* oscillator:{&native,&corrected})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=4;
    }
    for(int frame=0;frame<4096;++frame)
    {
        const float depth=frame%64<32 ? 16384.25f : 65536.75f;
        const auto render=[&](auto& oscillator,bool correcting) {
            return oscillator.processSample({
                .firstFrequency=261.625f, .secondFrequency=261.625f, .mix=.5f,
                .fmDepth=static_cast<float>(depth), .sync=true, .ring=true, .subOctave=0,
                .correctFmPhase=correcting
            });
        };
        EXPECT_EQ(render(native,false),render(corrected,true));
        EXPECT_EQ(native.readPhaseWords(),corrected.readPhaseWords());
    }
}

TEST(FmPhaseCorrection, StaticMappingPreservesKnotsAndFractionalEdits)
{
    const auto depthForNormalizedDial=[](float value) -> float {
        return synth1x::dsp::FrequencyModulation::depth(
            static_cast<float>(synth1x::dsp::fractionalDial(value)),0.0f);
    };
    const auto knot=[](int dial) {
        const float normalized=static_cast<float>(dial)*(1.f/127.f);
        return static_cast<int32_t>((std::exp(std::log(1000.)*normalized)-1.)/999.*67108864.);
    };
    for(int dial=0;dial<=127;++dial)
    {
        EXPECT_DOUBLE_EQ(depthForNormalizedDial(dial/127.f),static_cast<float>(knot(dial)));
        EXPECT_DOUBLE_EQ(depthForNormalizedDial(dial*(1.f/127.f)),static_cast<float>(knot(dial)));
    }
    double previous=depthForNormalizedDial(64.f/127.f);
    for(float dial:{64.1f,64.25f,64.49f,64.51f,64.75f})
    {
        const double value=depthForNormalizedDial(dial/127.f);
        EXPECT_GT(value,previous);EXPECT_LT(value,knot(65));previous=value;
    }
}

TEST(FmPhaseCorrection, AudioAdapterUsesFloatDepthWithCompatibleSyncReference)
{
    synth1x::dsp::OscillatorSection adapted,wide,narrow;
    for(auto* oscillator:{&adapted,&wide,&narrow})
    {
        oscillator->prepare({44100,64,2});
        oscillator->params.osc1Wave=3;oscillator->params.osc2Wave=4;
    }
    synth1x::dsp::OscillatorAudioInput input{};
    input.firstFrequency=input.secondFrequency=261.625f;
    input.pulseWidth=.5f;input.mix=1;input.sync=true;// This coordinate spans distinct float-product knots at wide depth.
    input.fm.depth=67108861.25;
    synth1x::dsp::OscillatorAudioSettings config{};config.correctFmPhase=true;
    config.subOctave=0;
    bool phaseDiffers=false;
    for(int frame=0;frame<4096;++frame)
    {
        const auto render=[&](auto& oscillator,double depth) {
            return oscillator.processSample({
                .firstFrequency=261.625f, .secondFrequency=261.625f, .mix=1, .fmDepth=static_cast<float>(depth),
                .sync=true, .subOctave=0, .correctFmPhase=true
            });
        };
        const double output=synth1x::dsp::OscillatorAudioRenderer::process(adapted,config,input,{},0,nullptr);
        EXPECT_EQ(output,render(wide,input.fm.depth));
        EXPECT_EQ(output,render(narrow,static_cast<float>(input.fm.depth)));
        EXPECT_EQ(adapted.getOsc1Phase(),wide.getOsc1Phase());
        phaseDiffers|=wide.getOsc1Phase()!=narrow.getOsc1Phase();
    }
    EXPECT_FALSE(phaseDiffers);
}

TEST(FmPhaseCorrection, IntegerDepthKnotsKeepFractionalSourceNativeIncrements)
{
    for(double depth:{0.,1.,12345.,67108863.})for(float source:{-.731f,-.123f,.123f,.731f})
    {
        synth1x::dsp::FmPhaseCorrection state;uint32_t phase=1234;
        const auto native=static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(depth)*source));
        for(int i=0;i<128;++i){auto result=state.advance(depth,source,400000,phase,true);
            EXPECT_EQ(result.audibleOffset,native);EXPECT_EQ(result.referenceOffset,native);
            phase=(phase+400000+result.audibleOffset)&mask;EXPECT_EQ(phase,result.referenceAfter);}
    }
}
TEST(FmPhaseCorrection, FractionalDepthInterpolatesAdjacentIntegerProductKnots)
{
    for(double depth:{12345.25,12345.75,-12345.25})for(float source:{-.731f,.731f})
    {
        const int low=static_cast<int>(std::floor(depth));
        const double first=static_cast<int32_t>(static_cast<float>(low)*source);
        const double second=static_cast<int32_t>(static_cast<float>(low+1)*source);
        const double target=first+(second-first)*(depth-low);
        const int native=static_cast<int32_t>(static_cast<float>(depth)*source);
        synth1x::dsp::FmPhaseCorrection state;uint32_t phase=0;int64_t delta=0;
        for(int i=0;i<1024;++i){auto result=state.advance(depth,source,400000,phase,true);
            delta+=offset(result.audibleOffset)-offset(result.referenceOffset);
            phase=(phase+400000+result.audibleOffset)&mask;}
        EXPECT_NEAR(static_cast<double>(delta),1024*(target-native),1.0);
    }
}
