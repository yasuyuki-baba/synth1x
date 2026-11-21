#include "Synth1X.h"
#include "host/PresetSession.h"
#include "IPlug_include_in_plug_src.h"
#include "ParameterSetup.h"
#if defined(VST3_API)
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#endif
#if IPLUG_EDITOR
#include "editor/EditorPaletteWindow.h"
#endif
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace iplug;

Synth1X::Synth1X(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(synth1x::host::ProcessorBridge::parameterCount+1, 1))
{
    for (int index=0; index<synth1x::host::ProcessorBridge::parameterCount; ++index)
    {
        synth1x::host::initializeHostParameter(*GetParam(index),index,processor.parameter(index));
    }
    GetParam(qualityParameter)->InitEnum("Oversampling",0,{"1x","2x","4x","8x","16x"},IParam::kFlagCannotAutomate,"Quality");
    syncParametersFromProcessor();browser.capture(processor);comparison.reset(processor,browser);
#if defined(APP_API)
    const auto session=synth1x::host::PresetFiles::loadStandaloneState();
    if(!session.empty()){IByteChunk chunk;chunk.PutBytes(session.data(),session.size());UnserializeState(chunk,0);}
#endif
#if IPLUG_EDITOR
    mMakeGraphicsFunc = [this]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                            editorScale);
    };
    mLayoutFunc = [this](igraphics::IGraphics* graphics) { layoutEditor(graphics); };
#endif
}

void Synth1X::syncParametersFromProcessor()
{
    for (int index=0; index<synth1x::host::ProcessorBridge::parameterCount; ++index)
        GetParam(index)->SetNormalized(processor.normalizedValue(index));
    int choice=0;while((1<<choice)<processor.oversamplingFactor())++choice;
    GetParam(qualityParameter)->Set(choice);
}

void Synth1X::OnReset()
{
    if (GetSampleRate() > 0 && GetBlockSize() > 0)
    {
        processor.prepare(GetSampleRate(), GetBlockSize());
        SetTailSize(static_cast<int>(std::min<double>(std::numeric_limits<int>::max(),
            std::ceil(GetSampleRate()*synth1x::host::ProcessorBridge::tailLengthSeconds))));
        syncQualityLatency();
        prepared = true;
    }
}
void Synth1X::OnActivate(bool active)
{
    if (!active) { processor.release(); prepared = false; }
    else if (!prepared) OnReset();
}
void Synth1X::syncQualityLatency()
{
    const int next=processor.latencySamples();
    if(next!=GetLatency())SetLatency(next);
}
void Synth1X::OnParamChange(int index)
{
    if(index==qualityParameter)processor.setOversamplingFactor(1<<GetParam(index)->Int());
    else processor.setNormalizedValue(index, GetParam(index)->GetNormalized());
}
void Synth1X::OnParamChange(int index, EParamSource source, int)
{
    if(source==kHost && index>=0 && index<synth1x::host::ProcessorBridge::parameterCount)
    {
        const auto* parameter=GetParam(index);
        // Hosts can echo every published value after a patch load. Compare the
        // SDK presentation (including discrete stepping) at bridge precision:
        // an unchanged echo must not replace legacy-converted controls or round
        // a fractional recalled selector. Explicit UI/native setters still run.
        const float published=static_cast<float>(parameter->ConstrainNormalized(processor.normalizedValue(index)));
        if(static_cast<float>(parameter->GetNormalized())==published)return;
    }
    OnParamChange(index);
}
void Synth1X::OnParamReset(EParamSource source)
{
    // iPlug2 broadcasts resets at initialization/activation. Replaying every
    // unchanged source setter would change native LFO/load history. Restore
    // already synchronizes DSP and parameters transactionally below.
    for (int index=0; index<synth1x::host::ProcessorBridge::parameterCount; ++index) OnParamChangeUI(index, source);
}
void Synth1X::ProcessMidiMsg(const IMidiMsg& message)
{
    if (message.StatusMsg() == IMidiMsg::kProgramChange)
    {
        if (message.mData1 < synth1x::host::PatchCatalog::programCount)
            pendingMidiProgram.store(message.mData1, std::memory_order_relaxed);
        return;
    }
    processor.enqueueMidi(message.mOffset, message.mStatus, message.mData1, message.mData2);
}
void Synth1X::ProcessBlock(sample**, sample** outputs, int frames)
{
    processor.process(outputs ? outputs[0] : nullptr, outputs ? outputs[1] : nullptr, frames, GetTempo());
}

bool Synth1X::loadCatalogPatch(const synth1x::host::CatalogPatch& item)
{
    if (!item.error.empty()) {
        browserStatus = item.error;
        return false;
    }
    std::string error;
    if (!processor.loadProgram(item.patch.text, item.program, error, item.patch.name)) {
        browserStatus = error;
        return false;
    }
    selectedPatchId = item.id;
    keyboardPatchId = item.id;
    currentPatch = item.patch.name;
    browserStatus.clear();
    stateRestored.store(false);
    browser.capture(processor);
    notifyHostOfPatch();
    return true;
}

void Synth1X::pollMidiProgram()
{
    const int requested = pendingMidiProgram.exchange(-1, std::memory_order_relaxed);
    if (requested >= 0)
        waitingMidiProgram = requested;
    if (waitingMidiProgram < 0 || libraryJob.valid())
        return;

    const int number = waitingMidiProgram;
    std::string error;
    const auto id = catalog.readProgramAssignment(number, error);
    const auto* patch = id.empty() ? nullptr : catalog.findPatch(id);
#if IPLUG_EDITOR
    if (!id.empty() && !patch && !midiProgramScanRequested) {
        midiProgramScanRequested = true;
        reloadCatalog();
        return;
    }
#endif
    midiProgramScanRequested = false;
    waitingMidiProgram = -1;
    if (patch) {
        if (loadCatalogPatch(*patch))
            browserGeneration.fetch_add(1);
    } else if (!error.empty()) {
        browserStatus = std::move(error);
    } else if (!id.empty()) {
        browserStatus = "The sound assigned to PC " + std::to_string(number) + " is missing.";
    } else {
        return;
    }
#if IPLUG_EDITOR
    refreshBrowser();
#endif
}

#if defined(VST3_API)
namespace {
using namespace Steinberg;
using namespace Steinberg::Vst;

bool isMidiControllerParameter(ParamID id)
{
    return id >= kMIDICCParamStartIdx && id < kMIDICCParamStartIdx + VST3_NUM_CC_CHANS * kCountCtrlNumber;
}

// iPlug2 processes the last point when it visits each queue. Deliver earlier
// MIDI points at that visit, preserving queue order and the SDK's editor
// delivery of the final point (including the keyboard's All Notes Off).
class Vst3InputParameterChanges final : public IParameterChanges
{
public:
    Vst3InputParameterChanges(IParameterChanges& source, Synth1X& plugin)
        : source(source), plugin(plugin) {}
    int32 PLUGIN_API getParameterCount() override { return source.getParameterCount(); }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override
    {
        auto* queue = source.getParameterData(index);
        if (!queue || !isMidiControllerParameter(queue->getParameterId()))
            return queue;

        const int midiIndex = queue->getParameterId() - kMIDICCParamStartIdx;
        const int channel = midiIndex / kCountCtrlNumber;
        const int controller = midiIndex % kCountCtrlNumber;
        const int last = queue->getPointCount() - 1;
        for (int point = 0; point < last; ++point) {
            int32 offset;
            double value;
            if (!readPoint(*queue, point, offset, value))
                continue;
            IMidiMsg message;
            if (controller == kAfterTouch)
                message.MakeChannelATMsg(static_cast<int>(value * 127.0), offset, channel);
            else if (controller == kPitchBend)
                message.MakePitchWheelMsg(value * 2.0 - 1.0, channel, offset);
            else
                message.MakeControlChangeMsg(static_cast<IMidiMsg::EControlChangeMsg>(controller), value, channel, offset);
            plugin.ProcessMidiMsg(message);
        }
        int32 offset;
        double value;
        return last >= 0 && readPoint(*queue, last, offset, value) ? queue : nullptr;
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override { return nullptr; }
    tresult PLUGIN_API queryInterface(const TUID, void** object) override
    {
        *object = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
private:
    static bool readPoint(IParamValueQueue& queue, int32 point, int32& offset, double& value)
    {
        return queue.getPoint(point, offset, value) == kResultTrue &&
            std::isfinite(value) && value >= 0.0 && value <= 1.0;
    }

    IParameterChanges& source;
    Synth1X& plugin;
};
}

Steinberg::tresult PLUGIN_API Synth1X::initialize(Steinberg::FUnknown* context)
{
    const auto result = Plugin::initialize(context);
    if (result != Steinberg::kResultOk)
        return result;

    using namespace Steinberg::Vst;
    auto* programs = new StringListParameter(STR16("MIDI Program"), kPresetParam, nullptr,
        ParameterInfo::kIsProgramChange | ParameterInfo::kIsList);
    for (int number = 0; number < synth1x::host::PatchCatalog::programCount; ++number) {
        String128 name {};
        getProgramName(kPresetParam, number, name);
        programs->appendString(name);
    }
    parameters.addParameter(programs);
    return result;
}

Steinberg::tresult PLUGIN_API Synth1X::process(Steinberg::Vst::ProcessData& data)
{
    using namespace Steinberg::Vst;
    auto* changes = data.inputParameterChanges;
    if (!changes)
        return Plugin::process(data);

    for (int i = 0; i < changes->getParameterCount(); ++i) {
        auto* queue = changes->getParameterData(i);
        if (!queue)
            continue;
        const auto id = queue->getParameterId();
        const int count = queue->getPointCount();
        if (id == kPresetParam) {
            // A host can send Program Change in a zero-frame callback.
            Steinberg::int32 offset;
            double value;
            if (count > 0 && queue->getPoint(count - 1, offset, value) == Steinberg::kResultTrue &&
                std::isfinite(value) && value >= 0.0 && value <= 1.0)
                pendingMidiProgram.store(static_cast<int>(std::lround(
                    value * (synth1x::host::PatchCatalog::programCount - 1))), std::memory_order_relaxed);
        }
    }

    Vst3InputParameterChanges inputChanges(*changes, *this);
    auto inputData = data;
    inputData.inputParameterChanges = &inputChanges;
    return Plugin::process(inputData);
}

Steinberg::tresult PLUGIN_API Synth1X::getMidiControllerAssignment(Steinberg::int32 bus,
    Steinberg::int16 channel, Steinberg::Vst::CtrlNumber controller, Steinberg::Vst::ParamID& id)
{
    if (channel < 0 || controller < 0 || controller >= Steinberg::Vst::kCountCtrlNumber)
        return Steinberg::kResultFalse;
    return Plugin::getMidiControllerAssignment(bus, channel, controller, id);
}

Steinberg::tresult PLUGIN_API Synth1X::setParamNormalized(Steinberg::Vst::ParamID tag,
                                                        Steinberg::Vst::ParamValue value)
{
    // The host's controller-side update only reflects the choice. The
    // processor callback queues recall, avoiding a second patch load.
    if (tag == kPresetParam)
        return Steinberg::Vst::SingleComponentEffect::setParamNormalized(tag, value);
    return Plugin::setParamNormalized(tag, value);
}

Steinberg::tresult PLUGIN_API Synth1X::getUnitInfo(Steinberg::int32 index, Steinberg::Vst::UnitInfo& info)
{
    const auto result = Plugin::getUnitInfo(index, info);
    if (result == Steinberg::kResultTrue && info.id == Steinberg::Vst::kRootUnitId)
        info.programListId = kPresetParam;
    return result;
}

Steinberg::tresult PLUGIN_API Synth1X::getUnitByBus(Steinberg::Vst::MediaType type,
    Steinberg::Vst::BusDirection direction, Steinberg::int32 bus, Steinberg::int32 channel,
    Steinberg::Vst::UnitID& unit)
{
    using namespace Steinberg::Vst;
    if (type != kEvent || direction != Steinberg::Vst::kInput || bus != 0 || channel < 0 ||
        channel >= VST3_NUM_MIDI_IN_CHANS)
        return Steinberg::kResultFalse;
    unit = kRootUnitId;
    return Steinberg::kResultTrue;
}

Steinberg::int32 PLUGIN_API Synth1X::getProgramListCount()
{
    return 1;
}

Steinberg::tresult PLUGIN_API Synth1X::getProgramListInfo(Steinberg::int32 index,
                                                        Steinberg::Vst::ProgramListInfo& info)
{
    if (index != 0)
        return Steinberg::kResultFalse;
    info.id = kPresetParam;
    info.programCount = synth1x::host::PatchCatalog::programCount;
    Steinberg::UString(info.name, 128).fromAscii("MIDI Programs");
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API Synth1X::getProgramName(Steinberg::Vst::ProgramListID list,
                                                    Steinberg::int32 number, Steinberg::Vst::String128 name)
{
    if (list != kPresetParam || number < 0 || number >= synth1x::host::PatchCatalog::programCount)
        return Steinberg::kResultFalse;
    Steinberg::UString(name, 128).fromAscii(("PC " + std::to_string(number)).c_str());
    return Steinberg::kResultTrue;
}
#endif

synth1x::host::PresetBrowserState Synth1X::captureBrowser() const
{
    auto state=browser;state.query=libraryFilter.query;state.bankId=libraryFilter.bankId;state.tag=libraryFilter.tag;state.selectedTags=libraryFilter.selectedTags;
    state.favoritesOnly=libraryFilter.favoritesOnly;state.restricted=libraryFilter.registeredOnly;
    state.importedIds=libraryFilter.registeredIds;state.selectedId=selectedPatchId;state.patchName=currentPatch;
    state.scroll=static_cast<int>(resultOffset);state.page=page;return state;
}
void Synth1X::restoreBrowser()
{
    libraryFilter.query=browser.query;libraryFilter.bankId=browser.bankId;libraryFilter.tag.clear();libraryFilter.selectedTags=browser.selectedTags;
    if(libraryFilter.selectedTags.empty() && !browser.tag.empty())libraryFilter.selectedTags.push_back(browser.tag);
    libraryFilter.favoritesOnly=browser.favoritesOnly;libraryFilter.registeredOnly=browser.restricted;
    libraryFilter.registeredIds=browser.importedIds;selectedPatchId=browser.selectedId;currentPatch=browser.patchName;
    resultOffset=browser.scroll;page=browser.page;
}
Synth1X::~Synth1X()
{
#if IPLUG_EDITOR
    closePaletteEditor();
#endif
    if(importPrompt){{std::lock_guard lock(importPrompt->mutex);importPrompt->alive=false;}importPrompt->changed.notify_all();}
#if defined(APP_API)
    try {std::string error;synth1x::host::PresetFiles::saveStandaloneState(synth1x::host::savePresetSession(processor,captureBrowser(),&comparison),error);}catch(const std::exception&){}
#endif
}
bool Synth1X::SerializeState(IByteChunk& chunk) const
{
    const auto bytes=synth1x::host::savePresetSession(processor,captureBrowser(),&comparison);chunk.PutBytes(bytes.data(),bytes.size());return true;
}
int Synth1X::UnserializeState(const IByteChunk& chunk,int start)
{
    if(start<0 || start>chunk.Size())return -1;
    const int n=synth1x::host::restorePresetSession(chunk.GetData()+start,chunk.Size()-start,processor,browser,&comparison);
    if(n<0)return -1;
    browserGeneration.fetch_add(1);restoreBrowser();syncParametersFromProcessor();syncQualityLatency();stateRestored.store(true);return start+n;
}

void Synth1X::notifyHostOfPatch()
{
    syncParametersFromProcessor();
    // The framework synchronizes format-specific host readback before sending
    // notifications, without replaying setters into the loaded processor.
    DirtyParametersFromUI();
    SendCurrentParamValuesFromDelegate();
}

bool Synth1X::selectComparison(int slot)
{
    return changeComparison(slot, ComparisonAction::select);
}

bool Synth1X::copyComparison(int source)
{
    return changeComparison(source, ComparisonAction::copy);
}

bool Synth1X::changeComparison(int slot, ComparisonAction action)
{
    const bool selecting = action == ComparisonAction::select;
    if (libraryJob.valid() || (selecting && slot == comparison.activeSlot()))
        return false;

    browser.selectedId = selectedPatchId;
    browser.patchName = currentPatch;
    const bool changed = selecting ? comparison.select(slot, processor, browser)
                                   : comparison.copyFrom(slot, processor, browser);
    if (!changed)
        return false;

    selectedPatchId = browser.selectedId;
    currentPatch = browser.patchName;
    keyboardPatchId.clear();
    browserStatus.clear();
    browserGeneration.fetch_add(1);
    notifyHostOfPatch();
    return true;
}

#if IPLUG_EDITOR
namespace synth1x::editor::native
{
iplug::igraphics::IGraphics* makePaletteGraphics(iplug::igraphics::IGEditorDelegate& delegate)
{
    return iplug::igraphics::MakeGraphics(delegate,EditorPaletteWindow::width,EditorPaletteWindow::height,PLUG_FPS,1.f);
}
}
#endif
