#pragma once
#include "audio/VoiceBank.h"
#include "midi/Arpeggiator.h"
#include "midi/ControllerRuntime.h"
#include "dsp/PitchBendSmoother.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace synth1x
{
    struct RenderControls;
    class SynthVoice;
    class VoiceCorePool;
    namespace dsp { class SharedModulationRuntime; }

    class VoiceDispatcher : public ::synth1x::audio::VoiceBank
    {
    public:
        explicit VoiceDispatcher(dsp::CrtRandom& random) noexcept : arpeggiator(random) {}
        RenderControls* params = nullptr;
        VoiceCorePool* corePool = nullptr;
        midi::Arpeggiator arpeggiator;
        dsp::SharedModulationRuntime* modulation=nullptr;
        const std::array<midi::ControllerRoute,2>* controllerSettings=nullptr;
        midi::ControllerRuntime controllers;

        void noteOn (int midiChannel, int midiNoteNumber, float velocity) override;
        void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;
        void allNotesOff (int midiChannel, bool allowTailOff) override;
        void handleSustainPedal(int midiChannel,bool down) override;
        void handlePitchWheel(int midiChannel,int value) override;
        void handleChannelPressure(int midiChannel, int pressure) override;
        void handleController(int midiChannel,int number,int value) override;
        std::array<int,4> nativeBendWords() const noexcept
        {return pitchBendWords(1);}
        std::array<int,4> pitchBendWords(int channel) const noexcept
        {const auto& bend=pitchBends[static_cast<size_t>(std::clamp(channel,1,16)-1)];return {bend.current,bend.target,bend.moving() ? 1 : 0,bend.offset};}
        void setPlaybackRate (float rate, bool preserveNotes);
        void resetAllocation() noexcept { lastAllocatedVoice = -1; }
        bool renderedVoicesInRegion {false};
        bool nativeOmniInput {};
        using OutputSink=void(*)(void*,int,int,bool);
        void renderRegion (::synth1x::audio::AudioBuffer<float>&, const ::synth1x::audio::MidiBuffer&, int startSample, int numSamples,
                           void* outputContext=nullptr,OutputSink outputSink=nullptr);
    protected:
        ::synth1x::audio::Voice* findFreeVoice (::synth1x::audio::Sound*, int, int, bool) const override;
    private:
        SynthVoice* soundingMonoVoice(int channel) const noexcept;
        bool resumeHeldMonoKey(int channel,SynthVoice&);
        bool preservingNotesForRateChange {};
        bool dispatchingArpeggiator {};
        void* activeOutputContext=nullptr;
        OutputSink activeOutputSink=nullptr;
        void rememberKeyDown(int channel,int note,float velocity);
        void trackLfoKey(const ::synth1x::audio::MidiMessage&);
        void applyControllerMessage(int status,int first,int second);
        void renderVoicesWithLfos (::synth1x::audio::AudioBuffer<float>&, int startSample, int numSamples,bool completeGainDispatch=false);
        std::array<std::array<uint64_t, 128>, 16> heldOrder {};
        std::array<std::array<float, 128>, 16> heldVelocity {};
        std::array<std::array<bool,128>,16> physicalKeys {};
        std::array<bool,16> sustainDown {};
        std::array<std::array<bool,128>,16> inputKeys {};
        std::array<bool,16> inputSustain {};
        std::array<dsp::PitchBendSmoother,16> pitchBends;
        std::array<SynthVoice*, 16> monoVoices {};
        uint64_t noteOrder = 0;
        mutable int lastAllocatedVoice = -1;
        int nativeWorkspaceOrigin {};
    };

}
