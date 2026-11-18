#include "PatchComparison.h"
#include "ProcessorState.h"
#include <utility>

namespace synth1x::host
{
PatchComparisonSound PatchComparison::capture(const ProcessorBridge& processor,const PresetBrowserState& browser)
{
    PatchComparisonSound result;
    result.state=processor.saveState();
    result.identity.selectedId=browser.selectedId;
    result.identity.patchName=browser.patchName;
    result.identity.baseline=browser.baseline;
    return result;
}
void PatchComparison::reset(const ProcessorBridge& processor,const PresetBrowserState& browser)
{
    alternate=capture(processor,browser);active=0;
}
bool PatchComparison::applyOther(ProcessorBridge& processor,PresetBrowserState& browser) const
{
    std::array<ParameterInfo,ProcessorBridge::parameterCount> info;
    for(int i=0;i<processor.parameterCount;++i)info[i]=processor.parameter(i);
    auto state=decodeProcessorState(alternate.state.data(),static_cast<int>(alternate.state.size()),info);
    if(!state)return false;
    // Quality is global. Publish a state with the current factor so rendering
    // never observes a transient request for the stored slot's old quality.
    state->oversamplingFactor=processor.oversamplingFactor();
    const auto bytes=encodeProcessorState(*state,info);
    if(!processor.tryRestoreState(bytes.data(),static_cast<int>(bytes.size())))return false;
    browser.selectedId=alternate.identity.selectedId;
    browser.patchName=alternate.identity.patchName;
    browser.baseline=alternate.identity.baseline;
    return true;
}
bool PatchComparison::select(int slot,ProcessorBridge& processor,PresetBrowserState& browser)
{
    if(slot<0 || slot>1)return false;
    if(slot==active)return true;
    auto previous=capture(processor,browser);
    if(!applyOther(processor,browser))return false;
    alternate=std::move(previous);active=slot;return true;
}
bool PatchComparison::copyFrom(int slot,ProcessorBridge& processor,PresetBrowserState& browser)
{
    if(slot<0 || slot>1)return false;
    if(slot==active)alternate=capture(processor,browser);
    else if(!applyOther(processor,browser))return false;
    return true;
}
void PatchComparison::recall(int slot,PatchComparisonSound other)
{
    active=slot;alternate=std::move(other);
}
}
