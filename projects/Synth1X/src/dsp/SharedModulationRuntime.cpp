#include "SharedModulationRuntime.h"
#include "FractionalDial.h"
#include "LfoShape.h"
#include "ReceiverControls.h"

namespace synth1x::dsp
{
    void SharedModulationRuntime::retimeClocks(float sampleRate)
    { if(!initial)for(auto& slot:slots)slot.clock.setSampleRate(static_cast<int>(sampleRate)); }
    void SharedModulationRuntime::prepareBlock(int capacity)
    {
        sharedBlock.frameCapacity=std::clamp(capacity,1,256);
        sharedBlock.startSample=0;
    }
    void SharedModulationRuntime::beginChunk(int start)
    { sharedBlock.startSample=start; }
    void SharedModulationRuntime::releaseKey(int channel,int note) noexcept
    { if(channel>=1 && channel<=16 && note>=0 && note<128)heldKeys[channel-1][note]=false; }
    void SharedModulationRuntime::releaseKeys(int channel) noexcept
    { for(int c=0;c<16;++c)if(channel==0 || channel==c+1)heldKeys[c].fill(false); }
    void SharedModulationRuntime::trackKey(const ::synth1x::audio::MidiMessage& message,const SharedModulationSettings& settings)
    {
        const int midiChannel=message.getChannel(),midiNoteNumber=message.getNoteNumber();
        if (message.isNoteOff() && midiChannel>=1 && midiChannel<=16)
            heldKeys[static_cast<size_t>(midiChannel-1)][static_cast<size_t>(midiNoteNumber)]=false;
        if (message.isAllNotesOff() || message.isAllSoundOff())
            for(int channel=0;channel<16;++channel)
                if(midiChannel==0 || midiChannel==channel+1)heldKeys[channel].fill(false);
        if (!message.isNoteOn()) return;
        if (midiChannel >= 1 && midiChannel <= 16 && midiNoteNumber >= 0 && midiNoteNumber < 128)
        {
            bool held = false;
            for (const auto& channel : heldKeys)
                held = held || std::any_of(channel.begin(),channel.end(),[](bool value) { return value; });
            heldKeys[static_cast<size_t>(midiChannel-1)][static_cast<size_t>(midiNoteNumber)] = true;
            // Key sync belongs to the first held upstream key, across every
            // play mode and receiver backend; overlapping keys do not restart it.
            if (!held)
            {
                const LfoSettings* controls[] {&settings.lfos[0],&settings.lfos[1]};
                for (size_t i = 0; i < 2; ++i)
                {
                    // The native first-key gate resets key-synced clocks even
                    // while their modulation is disabled.
                    if (controls[i]->keySync)
                    {
                        slots[i].clock.resetPhase();
                        random.resetPhase(i,slots[i].clock.counter,
                                fractionalDial(controls[i]->amount));
                    }
                }
            }
        }
    }

    void SharedModulationRuntime::updateRate(size_t slot,float dial,float bpm) noexcept
    {
        if(slots[slot].tempoSync)slots[slot].clock.setSynchronizedRate(dial,bpm);
        else slots[slot].clock.setRate(dial);
    }

    void SharedModulationRuntime::applyLoadedRate(size_t slot,float dial,bool tempoSync,int routingRateCalls,float bpm) noexcept
    {
        // MIDI routes and the ordinary rate setter use the retained sync mode.
        for(int call=0;call<1+routingRateCalls;++call)updateRate(slot,dial,bpm);
        slots[slot].tempoSync=tempoSync;
        updateRate(slot,dial,bpm);
    }

    void SharedModulationRuntime::applyHistory(const LfoHistoryEvent& event,float bpm) noexcept
    {
        using Kind=LfoHistoryEvent::Kind;
        if(event.kind==Kind::RandomEdit){random.edit(event.a,event.b);return;}
        if(event.kind==Kind::Load || event.kind==Kind::LoadWithMidiRouting)
        {
            // A subsequent program transaction after admitted random arp has
            // unverified keyboard/counter history. Do not invent a fresh seed.
            if(random.arpeggioSeen)random.valid=false;
            random.arpeggioSeen=random.arpeggioSeen || event.randomArpeggio;
            if(!event.randomAdmitted)random.valid=false;
            std::array<RandomLfoRuntime::Control,2> controls{};
            for(size_t i=0;i<2;++i)
            {
                const auto& source=event.randomControls[i];
                controls[i]={RandomLfoRuntime::decodeDestination(source[0]),source[1],source[2]};
            }
            random.load(event.routes,controls);
        }
        // Retain the established cold-start constructor protocol. Before the
        // first render there is no running clock; the final mapped load seeds it.
        if(initial)return;
        if(event.kind==Kind::Rate || event.kind==Kind::TempoSync)
        {
            const auto slot=static_cast<size_t>(event.a);
            if(slot>=2)return;
            if(event.kind==Kind::TempoSync)
            {
                slots[slot].tempoSync=event.b!=0;
                updateRate(slot,slots[slot].clock.rateDial,bpm);
            }
            else {updateRate(slot,event.b,bpm);slots[slot].previousRate=event.b;}
        }
        else
        {
            const auto calls=event.kind==Kind::LoadWithMidiRouting ? midiRouting.load(event.routes) : std::array<int,2>{};
            const int rates[]{event.a,event.b};
            for(size_t slot=0;slot<2;++slot)
            {
                applyLoadedRate(slot,rates[slot],event.sync[slot]!=0,calls[slot],bpm);
                slots[slot].previousRate=rates[slot];
            }
            loadAlreadyApplied=true;
        }
    }

    void SharedModulationRuntime::synchronize(const SharedModulationSettings& settings,float sampleRate)
    {
        const LfoSettings* lfoControls[] {&settings.lfos[0],&settings.lfos[1]};
        std::array<int,2> routingRateCalls {};
        if ((initial || (reload && !loadAlreadyApplied)) && settings.midiRoutingLoadHistory)
            routingRateCalls=midiRouting.load(settings.midiRoutingControls);
        const bool tempoChanged=previousBpm!=settings.hostBpm;
        previousBpm=settings.hostBpm;
        for (size_t i = 0; i < 2; ++i)
        {
            auto& clock=slots[i].clock;
            const float dial=fractionalDial(lfoControls[i]->speed);
            if(initial)
            {
                clock=LfoClock{};slots[i].tempoSync=false;
                // Native construction applies rate and sync setters at44.1k,
                // followed by the host sample-rate setter.
                for (int call=0;call<2+routingRateCalls[i];++call) clock.setRate(dial);
                if(lfoControls[i]->tempoSync)
                {
                    slots[i].tempoSync=true;
                    clock.setSynchronizedRate(dial,settings.hostBpm);
                }
                clock.setSampleRate(static_cast<int>(sampleRate));
            }
            else if(reload && !loadAlreadyApplied)
            {
                applyLoadedRate(i,dial,lfoControls[i]->tempoSync,routingRateCalls[i],settings.hostBpm);
            }
            else if(slots[i].rateTouched || dial != slots[i].previousRate || (tempoChanged && slots[i].tempoSync))updateRate(i,dial,settings.hostBpm);
            slots[i].previousRate=dial;

        }
        initial = false;
        reload = false;
        loadAlreadyApplied=false;
        for(auto& slot:slots)slot.rateTouched=false;
    }
    int SharedModulationRuntime::randomBoundary(int count,const SharedModulationSettings& settings) const noexcept
    {
        int span=count;
        const LfoSettings* controls[]{&settings.lfos[0],&settings.lfos[1]};
        for(size_t slot=0;slot<2;++slot)
            if(controls[slot]->on && controls[slot]->wave>=4)
                span=std::min(span,slots[slot].clock.period-slots[slot].clock.counter);
        return span;
    }
    // Native graph 0xca70 re-queries LFO2 at LFO1 span boundaries
    // for these receiver combinations.
    static bool requerySecondLfoAtFirstBoundary(const SharedModulationSettings& settings) noexcept
    {
        const auto& first=settings.lfos[0];
        const auto& second=settings.lfos[1];
        const bool dualPitch=settings.pitchReceiver && first.on
            && (first.dest == LfoDestination::Oscillator2 || first.dest == LfoDestination::BothOscillators)
            && (second.dest == LfoDestination::Oscillator2 || second.dest == LfoDestination::BothOscillators);
        const bool dualFilter=settings.filterReceiver && first.on
            && first.dest == LfoDestination::Filter && second.dest == LfoDestination::Filter;
        const bool dualAmplitude=first.on && second.on
            && first.dest == LfoDestination::Amplitude && second.dest == LfoDestination::Amplitude;
        const bool dualPulse=first.on && first.dest == LfoDestination::PulseWidth && second.dest == LfoDestination::PulseWidth;
        const bool nestedPulse=settings.pulseWidthReceiver && (settings.pitchReceiver || dualPulse);
        const bool nestedFm=(first.dest == LfoDestination::Fm || second.dest == LfoDestination::Fm)
            && ReceiverLfoView::reachesOscillator({first.on,first.dest})
            && ReceiverLfoView::reachesOscillator({second.on,second.dest});
        return dualPitch || dualFilter || dualAmplitude || nestedPulse || nestedFm;
    }

    void SharedModulationRuntime::renderSpan(int start,int count,const SharedModulationSettings& settings)
    {
        const LfoSettings* controls[] {&settings.lfos[0],&settings.lfos[1]};
        const int offset = start-sharedBlock.startSample;
        const bool requerySecondLfo=requerySecondLfoAtFirstBoundary(settings);
        for (int lfo = 0; lfo < 2; ++lfo)
        {
            auto* clock=&slots[lfo].clock;
            const LfoShape::DepthCurve depth(controls[lfo]->dest,fractionalDial(controls[lfo]->amount));
            // Receiver queries use the clock's actual synchronized step,
            // not the free-rate dial's threshold.
            const int step=clock->step;
            const bool followFirstSpan=lfo==1 && requerySecondLfo;
            int remaining=0;float target=0;bool muted=false;
            for (int i = 0; i < count; ++i)
            {
                if (remaining==0)
                {
                    // Native queries offset the retained counter using the current
                    // period; slew/rescaling occurs after the graph span.
                    const auto counter=static_cast<uint32_t>((clock->counter+i)%clock->period);
                    if(controls[lfo]->wave>=4)
                    {
                        const auto query=random.query(static_cast<size_t>(lfo),static_cast<int>(counter),clock->period,clock->step,fractionalDial(controls[lfo]->amount));
                        target=query.first;remaining=query.second<0 ? count-i : query.second;
                    }
                    else
                    {
                        muted=controls[lfo]->wave==0 && depth.sineMutesAmplitude(static_cast<int>(counter),clock->period);
                        const auto query=depth.query(controls[lfo]->wave,static_cast<int>(counter),clock->period,step);
                        target=query.first;remaining=query.second<0 ? count-i : query.second;
                    }
                }
                if(followFirstSpan)
                    remaining=std::min(remaining,sharedBlock.controlRemaining[0][offset+i]);
                sharedBlock.sineAmplitudeMute[lfo][offset+i]=muted;
                sharedBlock.controls[lfo][offset+i]=target;
                sharedBlock.controlRemaining[lfo][offset+i]=std::min(remaining,count-i);
                --remaining;
            }
            // Both native clocks advance even when the LFO is disabled.
            {
                random.advance(static_cast<size_t>(lfo),count,clock->counter,clock->period,
                    fractionalDial(controls[lfo]->amount));
                clock->advance(count);
            }
        }
    }
}
