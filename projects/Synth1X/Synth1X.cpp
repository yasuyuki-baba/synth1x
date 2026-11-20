#include "Synth1X.h"
#include "host/PresetSession.h"
#include "IPlug_include_in_plug_src.h"
#include "ParameterSetup.h"
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
    processor.enqueueMidi(message.mOffset, message.mStatus, message.mData1, message.mData2);
}
void Synth1X::ProcessBlock(sample**, sample** outputs, int frames)
{
    processor.process(outputs ? outputs[0] : nullptr, outputs ? outputs[1] : nullptr, frames, GetTempo());
}

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
