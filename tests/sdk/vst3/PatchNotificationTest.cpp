#include "Synth1X.h"
#include "patch/PatchFormat.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

// UI callbacks are outside this test. The constructor, state/parameter methods,
// patch notification, processor and VST3 host API are the production code.
void Synth1X::OnIdle() {}
void Synth1X::OnUIOpen() {}
void Synth1X::OnUIClose() {}
void Synth1X::OnParentWindowResize(int,int) {}

namespace
{
void require(bool condition,const char* message)
{
    if(!condition)throw std::runtime_error(message);
}
struct Host final : Steinberg::Vst::IComponentHandler
{
    Synth1X* plugin{};
    unsigned reads=0,mismatches=0,latencyChanges=0;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,void** result) override
    {
        if(Steinberg::FUnknownPrivate::iidEqual(iid,Steinberg::Vst::IComponentHandler::iid) ||
           Steinberg::FUnknownPrivate::iidEqual(iid,Steinberg::FUnknown::iid)) {
            *result=static_cast<Steinberg::Vst::IComponentHandler*>(this);addRef();return Steinberg::kResultOk;
        }
        *result=nullptr;return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID id,Steinberg::Vst::ParamValue value) override
    {
        ++reads;
        if(std::abs(plugin->getParamNormalized(id)-value)>1e-12)++mismatches;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID) override { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override
    {
        if(flags & Steinberg::Vst::kLatencyChanged)++latencyChanges;
        return Steinberg::kResultOk;
    }
};
}

struct Synth1XVst3PatchTest
{
    static void checkProgramChange()
    {
        using namespace Steinberg::Vst;
        namespace fs = std::filesystem;
        struct Library {
            fs::path root = fs::temp_directory_path() / ("synth1x-pc-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            ~Library() { std::error_code ignored; fs::remove_all(root, ignored); }
        } library;

        Host host;
        Synth1X plugin {iplug::InstanceInfo{}};
        host.plugin = &plugin;
        require(plugin.initialize(&host) == Steinberg::kResultOk, "Program Change initialize failed");
        plugin.setComponentHandler(&host);
        ProgramListInfo list {};
        UnitID unit = -1;
        UnitInfo unitInfo {};
        require(plugin.getProgramListCount() == 1 && plugin.getProgramListInfo(0, list) == Steinberg::kResultTrue &&
                list.programCount == 128, "Missing 128-slot VST3 program list");
        require(plugin.getUnitByBus(kEvent, kInput, 0, 15, unit) == Steinberg::kResultTrue &&
                plugin.getUnitInfo(0, unitInfo) == Steinberg::kResultTrue && unitInfo.id == unit &&
                unitInfo.programListId == list.id, "VST3 MIDI input has no Program Change unit");
        bool hasProgramParam = false;
        for (int index = 0; index < plugin.getParameterCount(); ++index) {
            ParameterInfo info {};
            if (plugin.getParameterInfo(index, info) == Steinberg::kResultTrue && info.id == list.id)
                hasProgramParam = (info.flags & ParameterInfo::kIsProgramChange) && info.stepCount == 127;
        }
        require(hasProgramParam, "Missing stepped VST3 program parameter");

        plugin.catalog = synth1x::host::PatchCatalog(library.root);
        std::string error, padId, leadId;
        require(plugin.catalog.saveFile("Pad in use.sy1", "Soft Pad\nver=113\n19,83\n", false, padId, error),
                "Could not save Program Change test pad");
        require(plugin.catalog.saveFile("First lead.sy1", "Bright Lead\nver=113\n19,103\n", false, leadId, error),
                "Could not save Program Change test lead");
        require(plugin.catalog.setFavorite(padId, true, error) && plugin.catalog.assignProgram(127, padId, error) &&
                plugin.catalog.setFavorite(leadId, true, error) && plugin.catalog.assignProgram(0, leadId, error),
                "Could not assign Program Change test patches");
        plugin.catalogLoaded = true;
        ProcessSetup setup {};
        setup.sampleRate = 48000;
        setup.maxSamplesPerBlock = 128;
        setup.symbolicSampleSize = kSample64;
        require(plugin.setupProcessing(setup) == Steinberg::kResultOk, "Program Change setup failed");
        plugin.setActive(true);
        require(plugin.setParamNormalized(list.id, 1.0) == Steinberg::kResultTrue, "Controller rejected program");

        ParameterChanges changes;
        Steinberg::int32 queueIndex = 0, pointIndex = 0;
        changes.addParameterData(list.id, queueIndex)->addPoint(0, 1.0, pointIndex);
        ProcessData data {};
        data.symbolicSampleSize = kSample64;
        data.inputParameterChanges = &changes;
        require(plugin.process(data) == Steinberg::kResultOk, "Zero-frame VST3 Program Change callback failed");
        require(plugin.currentPatch == "Init", "Program Change loaded from the audio callback");
        plugin.pollMidiProgram();
        require(plugin.currentPatch == "Soft Pad" && plugin.selectedPatchId == padId && host.reads > 0 &&
                host.mismatches == 0, "VST3 host Program Change did not publish the assigned pad");

        iplug::IMidiMsg raw {};
        raw.mStatus = 0xCF;
        raw.mData1 = 0;
        plugin.ProcessMidiMsg(raw);
        plugin.pollMidiProgram();
        require(plugin.currentPatch == "Bright Lead" && plugin.processor.currentPatchName() == "Bright Lead",
                "Raw channel-16 Program Change did not recall slot 0");
        const auto before = plugin.processor.saveState();
        raw.mData1 = 1;
        plugin.ProcessMidiMsg(raw);
        plugin.pollMidiProgram();
        require(plugin.processor.saveState() == before, "Unassigned Program Change altered the patch");
        plugin.setActive(false);
        plugin.setComponentHandler(nullptr);
        plugin.terminate();
    }

    static void checkHostEcho()
    {
        // Authored legacy input: loaded control conversion must survive a host
        // returning all of the values it just read from the controller.
        const std::string patch="Legacy host echo\nver=100\n0,1\n1,1\n19,110\n20,8\n21,72\n"
            "25,44\n26,96\n27,127\n28,48\n29,52\n30,90\n65,0\n66,0\n57,0\n58,0\n59,0\n73,0\n77,0\n";
        Synth1X baseline{iplug::InstanceInfo{}},echoed{iplug::InstanceInfo{}};
        for(auto* plugin:{&baseline,&echoed})
        {
            require(plugin->initialize(nullptr)==Steinberg::kResultOk,"Echo initialize failed");
            Steinberg::Vst::ProcessSetup setup{};
            setup.sampleRate=48000;setup.maxSamplesPerBlock=128;setup.symbolicSampleSize=Steinberg::Vst::kSample64;
            require(plugin->setupProcessing(setup)==Steinberg::kResultOk,"Echo setup failed");
            plugin->setActive(true);
            std::string error;
            require(plugin->processor.loadProgram(patch,0,error),"Echo legacy patch load failed");
            plugin->notifyHostOfPatch();
        }
        const auto before=echoed.processor.saveState();
        Steinberg::Vst::ParameterChanges changes;
        for(int i=0;i<echoed.NParams();++i)
        {
            Steinberg::int32 queueIndex=0,pointIndex=0;
            changes.addParameterData(i,queueIndex)->addPoint(0,echoed.getParamNormalized(i),pointIndex);
        }
        Steinberg::Vst::ProcessData data{};data.inputParameterChanges=&changes;
        iplug::IPlugQueue<iplug::IMidiMsg> outgoing{128};
        echoed.ProcessParameterChanges(data,outgoing);
        require(echoed.processor.saveState()==before,"Host echo changed loaded legacy controls/state");
        const auto a=energy(baseline),b=energy(echoed);
        require(a==b,"Host echo changed legacy patch audio");
        Steinberg::Vst::ParameterChanges edit;
        Steinberg::int32 queueIndex=0,pointIndex=0;
        edit.addParameterData(29,queueIndex)->addPoint(0,0,pointIndex);
        data.inputParameterChanges=&edit;echoed.ProcessParameterChanges(data,outgoing);
        require(echoed.processor.normalizedValue(29)==0,"Host gain edit was ignored");
        require(energy(echoed)==0,"Host gain edit did not silence the patch");
        const auto beforeUi=echoed.processor.saveState();
        echoed.OnParamChange(43,iplug::kUI);
        require(echoed.processor.saveState()!=beforeUi,"Explicit UI setter history was suppressed");
        for(auto* plugin:{&baseline,&echoed}){plugin->setActive(false);plugin->terminate();}
        std::cout<<"Legacy host parameter echo: state and audio unchanged\n";
    }
    static double energy(Synth1X& plugin)
    {
        iplug::IMidiMsg note;note.MakeNoteOnMsg(60,100,0);plugin.ProcessMidiMsg(note);
        std::array<iplug::sample,128> left{},right{};
        iplug::sample* outputs[] {left.data(),right.data()};
        double result=0;
        for(int block=0;block<32;++block) {
            plugin.ProcessBlock(nullptr,outputs,128);
            if(block>=16)for(int i=0;i<128;++i)result+=left[i]*left[i]+right[i]*right[i];
        }
        return result;
    }
    static void checkMidiTimeline(const std::string& tone)
    {
        using namespace Steinberg::Vst;
        Host host;
        Synth1X plugin {iplug::InstanceInfo{}};host.plugin=&plugin;
        require(plugin.initialize(&host)==Steinberg::kResultOk,"UI MIDI VST3 initialize failed");
        BusInfo midiBus {};
        require(plugin.getBusInfo(kEvent, kInput, 0, midiBus) == Steinberg::kResultTrue &&
                midiBus.channelCount == 16, "VST3 did not advertise all 16 MIDI channels");
        for (int channel = 0; channel < 16; ++channel) {
            for (CtrlNumber controller : std::array<CtrlNumber,4>{7,64,kAfterTouch,kPitchBend}) {
                ParamID id;
                require(plugin.getMidiControllerAssignment(0, channel, controller, id) == Steinberg::kResultTrue &&
                        id == iplug::kMIDICCParamStartIdx + channel * kCountCtrlNumber + controller,
                        "VST3 MIDI assignment missing or changed");
            }
        }
        ParamID unused;
        require(plugin.getMidiControllerAssignment(0, -1, 7, unused) == Steinberg::kResultFalse &&
                plugin.getMidiControllerAssignment(0, 16, 7, unused) == Steinberg::kResultFalse &&
                plugin.getMidiControllerAssignment(0, 0, kCountCtrlNumber, unused) == Steinberg::kResultFalse,
                "VST3 accepted an invalid MIDI assignment");

        const auto patch = tone + "19,40\n50,127\n86,53248\n87,19\n";
        synth1x::host::ProcessorBridge reference;
        std::string error;
        require(plugin.processor.loadProgram(patch,0,error),"UI MIDI patch load failed");
        require(reference.loadProgram(patch,0,error),"Reference MIDI patch load failed");
        Steinberg::Vst::ProcessSetup setup {};
        setup.sampleRate=48000;setup.maxSamplesPerBlock=128;setup.symbolicSampleSize=Steinberg::Vst::kSample64;
        require(plugin.setupProcessing(setup)==Steinberg::kResultOk,"UI MIDI VST3 setup failed");
        plugin.setActive(true);reference.prepare(48000,128);
        plugin.activateBus(Steinberg::Vst::kAudio,Steinberg::Vst::kOutput,0,true);
        plugin.setProcessing(true);
        std::array<double,128> left{},right{},expectedLeft{},expectedRight{};
        double* outputs[] {left.data(),right.data()};
        Steinberg::Vst::AudioBusBuffers bus {};
        bus.numChannels=2;bus.channelBuffers64=outputs;
        Steinberg::Vst::ProcessData data {};
        data.numSamples=128;data.numOutputs=1;data.outputs=&bus;
        data.symbolicSampleSize=Steinberg::Vst::kSample64;
        double sounding=0;
        for(int block=0;block<512;++block)
        {
            ParameterChanges changes;
            if (block == 2) {
                struct Point {
                    int controller, offset;
                    double normalized;
                    uint8_t status, data1, data2;
                };
                // Host queues group points by controller. The raw reference
                // uses the same queue order at equal sample offsets.
                const Point points[] {
                    {7, 11, 0.0, 0xBF, 7, 0},
                    {7, 95, 1.0, 0xBF, 7, 127},
                    {kPitchBend, 23, 1.0, 0xEF, 127, 127},
                    {kPitchBend, 79, 0.0, 0xEF, 0, 0},
                    {kPitchBend, 112, 0.5, 0xEF, 0, 64},
                    {kAfterTouch, 23, 1.0, 0xDF, 127, 0},
                    {kAfterTouch, 79, 0.0, 0xDF, 0, 0},
                    {64, 35, 1.0, 0xBF, 64, 127},
                    {64, 87, 0.0, 0xBF, 64, 0}
                };
                for (const auto& point : points) {
                    ParamID id;
                    require(plugin.getMidiControllerAssignment(0, 15, point.controller, id) == Steinberg::kResultTrue,
                            "MIDI controller could not be routed on channel 16");
                    Steinberg::int32 queueIndex, pointIndex;
                    changes.addParameterData(id, queueIndex)->addPoint(point.offset, point.normalized, pointIndex);
                    require(reference.enqueueMidi(point.offset, point.status, point.data1, point.data2),
                            "Reference rejected a MIDI controller point");
                }
            }
            data.inputParameterChanges = &changes;
            if (block == 2) {
                iplug::IMidiMsg off, on;
                off.MakeNoteOffMsg(60, 49);
                on.MakeNoteOnMsg(60, 100, 101);
                for (const auto& message : {off, on}) {
                    plugin.SendMidiMsgFromUI(message);
                    require(reference.enqueueMidi(message.mOffset, message.mStatus, message.mData1, message.mData2),
                            "Reference rejected an interleaved note");
                }
            }
            if(block==0 || block==4 || block==6 || block==8)
            {
                iplug::IMidiMsg message;
                if(block==0 || block==6)message.MakeNoteOnMsg(60,100,0);
                else message.MakeNoteOffMsg(60,0);
                plugin.SendMidiMsgFromUI(message);
                require(reference.enqueueMidi(message.mOffset,message.mStatus,message.mData1,message.mData2),
                        "Reference MIDI queue rejected a note");
            }
            if(block==0) {
                data.numSamples=0;
                require(plugin.process(data)==Steinberg::kResultOk,"Zero-frame VST3 callback failed");
                data.numSamples=128;
            }
            require(plugin.process(data)==Steinberg::kResultOk,"VST3 UI MIDI callback failed");
            require(reference.process(expectedLeft.data(),expectedRight.data(),128,120),"Reference MIDI render failed");
            require(left==expectedLeft && right==expectedRight,"VST3 controller or editor MIDI differs from the raw MIDI timeline");
            if(block<4)for(int i=0;i<128;++i)sounding+=left[i]*left[i]+right[i]*right[i];
        }
        require(sounding>1e-8,"Framework UI MIDI did not produce audio");
        for(int i=0;i<128;++i)require(left[i]==0 && right[i]==0,"Framework UI note-off did not release the note");
        plugin.setProcessing(false);plugin.setActive(false);plugin.terminate();
        std::cout << "VST3 16-channel controller and editor MIDI: audio matches the raw MIDI timeline\n";
    }
    static void run()
    {
        checkHostEcho();
        Host host;
        Synth1X plugin {iplug::InstanceInfo{}};host.plugin=&plugin;
        require(plugin.initialize(&host)==Steinberg::kResultOk,"VST3 initialize failed");
        plugin.setComponentHandler(&host);
        Steinberg::Vst::ProcessSetup setup {};
        setup.sampleRate=48000;setup.maxSamplesPerBlock=128;setup.symbolicSampleSize=Steinberg::Vst::kSample64;
        require(plugin.setupProcessing(setup)==Steinberg::kResultOk,"VST3 processing setup failed");
        plugin.setActive(true);
        const auto patch=[](int gain) {
            return std::string("VST3 synthetic patch\n0,1\n1,1\n19,127\n25,0\n26,0\n27,127\n28,0\n")+
                "29,"+std::to_string(gain)+"\n37,0\n56,0\n65,0\n66,0\n77,0\nver=113\n";
        };
        std::string error;
        require(plugin.processor.loadProgram(patch(100),0,error),"Sounding patch load failed");
        plugin.notifyHostOfPatch();
        const double sounding=energy(plugin);
        require(sounding>1e-8,"Generated sounding patch did not produce audio");
        require(plugin.processor.loadProgram(patch(0),1,error),"Silent patch load failed");
        const auto before=plugin.processor.saveState();
        plugin.notifyHostOfPatch();
        require(plugin.processor.saveState()==before,"Host notification changed loaded processor state");
        const double muted=energy(plugin);
        require(muted==0,"Loaded silent patch did not silence the engine");
        // A host may query the controller's current value and later return it
        // through process(inputParameterChanges). Exercise that real SDK path.
        Steinberg::Vst::ParameterChanges changes;
        Steinberg::int32 queueIndex=0,pointIndex=0;
        auto* queue=changes.addParameterData(29,queueIndex);
        queue->addPoint(0,plugin.getParamNormalized(29),pointIndex);
        Steinberg::Vst::ProcessData data {};data.inputParameterChanges=&changes;
        iplug::IPlugQueue<iplug::IMidiMsg> outgoing {128};
        plugin.ProcessParameterChanges(data,outgoing);
        const double echoed=energy(plugin);
        std::cout << "Audio energy: sounding=" << sounding << "; muted=" << muted << "; host readback echo=" << echoed << '\n';
        std::cout << "Host readbacks: " << host.reads << "; stale: " << host.mismatches << '\n';
        require(echoed==0,"Host readback echo restored the previous audible gain");
        require(host.reads==static_cast<unsigned>(plugin.NParams()*2),"Host did not receive both patches");
        for(int i=0;i<plugin.NParams();++i)
            require(std::abs(plugin.getParamNormalized(i)-plugin.GetParam(i)->GetNormalized())<1e-12,
                    "VST3 host readback differs from the loaded SDK parameter");
        require(host.mismatches==0,"VST3 exposed stale values during patch notification");
        // Discrete SDK presentation must not round fractional recalled bridge
        // controls back into the engine while publishing a patch.
        auto fractional=synth1x::parseContinuousPatch(plugin.processor.savePatch());
        fractional.values[64]=2.25f;fractional.values[19]=63.75f;
        require(plugin.processor.loadProgram(synth1x::formatContinuousPatch(fractional),2,error),"Fractional patch load failed");
        const auto fractionalBefore=plugin.processor.saveState();
        plugin.notifyHostOfPatch();
        require(plugin.processor.saveState()==fractionalBefore,"Notification rounded fractional recalled values");
        Steinberg::Vst::ParameterChanges fractionalEcho;
        for(int i=0;i<plugin.NParams();++i)
        {
            Steinberg::int32 queueIndex=0,pointIndex=0;
            fractionalEcho.addParameterData(i,queueIndex)->addPoint(0,plugin.getParamNormalized(i),pointIndex);
        }
        data.inputParameterChanges=&fractionalEcho;
        plugin.ProcessParameterChanges(data,outgoing);
        require(plugin.processor.saveState()==fractionalBefore,"Host echo rounded fractional recalled values");
        require(plugin.processor.setMidiDestination(0,-1),"MIDI destination disable failed");
        const auto disabledBefore=plugin.processor.saveState();
        plugin.notifyHostOfPatch();
        require(plugin.GetParam(87)->GetMin()==0 && plugin.GetParam(87)->GetMax()==127,
                "MIDI destination host range changed");
        require(plugin.processor.saveState()==disabledBefore,"Notification changed disabled MIDI routing");
        Steinberg::Vst::ParameterChanges disabledEcho;
        for(int i=0;i<plugin.NParams();++i)
        {
            Steinberg::int32 queueIndex=0,pointIndex=0;
            disabledEcho.addParameterData(i,queueIndex)->addPoint(0,plugin.getParamNormalized(i),pointIndex);
        }
        data.inputParameterChanges=&disabledEcho;
        plugin.ProcessParameterChanges(data,outgoing);
        require(plugin.processor.saveState()==disabledBefore,"Host echo changed disabled MIDI routing");
        require(plugin.processor.midiDestination(0)==-1,"Disabled MIDI destination became zero");
        // Repeated UI ticks must not repeatedly request a latency restart.
        for(int factor:{4,1})
        {
            const int previous=plugin.GetLatency();
            plugin.processor.setOversamplingFactor(factor);
            energy(plugin);
            const int next=plugin.processor.latencySamples();
            require(factor==1 ? next==0 : next>0,"Oversampling did not reach the requested latency");
            const auto notifications=host.latencyChanges;
            plugin.syncQualityLatency();
            require(plugin.GetLatency()==next,"SDK latency differs from the processor latency");
            require(host.latencyChanges==notifications+(previous!=next),"Latency change was not published once");
            plugin.syncQualityLatency();plugin.syncQualityLatency();
            require(host.latencyChanges==notifications+(previous!=next),"Unchanged latency requested another restart");
        }
        // A/B uses the same framework publication path as a loaded patch.
        require(plugin.processor.loadProgram(patch(100),0,error),"A reference load failed");
        plugin.currentPatch="Reference";plugin.selectedPatchId="reference";
        plugin.browser.capture(plugin.processor);plugin.browser.patchName=plugin.currentPatch;plugin.browser.selectedId=plugin.selectedPatchId;
        plugin.comparison.reset(plugin.processor,plugin.browser);
        plugin.libraryFilter.query="shared query";
        require(plugin.selectComparison(1),"Select B failed");
        require(plugin.processor.loadProgram(patch(0),1,error),"B candidate load failed");
        plugin.currentPatch="Candidate";plugin.selectedPatchId="candidate";plugin.browser.capture(plugin.processor);
        plugin.notifyHostOfPatch();
        require(energy(plugin)==0,"B was not silent");
        require(plugin.selectComparison(0),"Select A failed");
        require(plugin.currentPatch=="Reference" && plugin.libraryFilter.query=="shared query","A/B changed identity or shared filters");
        require(energy(plugin)>1e-8,"A/B did not restore audible A");
        require(plugin.selectComparison(1),"Return to B failed");
        require(energy(plugin)==0,"A/B did not restore silent B");
        require(host.mismatches==0,"A/B published stale VST3 controller values");
        require(plugin.copyComparison(0),"Copy A to B failed");
        require(plugin.comparison.activeSlot()==1 && energy(plugin)>1e-8,"Copy from A did not replace active B");
        iplug::IByteChunk comparisonState;
        require(plugin.SerializeState(comparisonState),"A/B serialization failed");
        require(plugin.selectComparison(0),"Select A before recall failed");
        require(plugin.UnserializeState(comparisonState,0)==comparisonState.Size(),"A/B project recall failed");
        require(plugin.comparison.activeSlot()==1 && plugin.currentPatch=="Reference","A/B recall lost selection or identity");
        require(host.mismatches==0,"A/B copy published stale VST3 values");
        std::cout<<"A/B: audible/silent switching, copy, host readback and project recall passed\n";
        checkMidiTimeline(patch(100));
        checkProgramChange();
        plugin.setActive(false);
        plugin.setComponentHandler(nullptr);plugin.terminate();
    }
};
int main()
{
    try { Synth1XVst3PatchTest::run(); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
