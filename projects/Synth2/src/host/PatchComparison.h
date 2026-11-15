#pragma once
#include "PresetFiles.h"

namespace synth2::host
{
// Comparison snapshots contain sound/loading state and its edit baseline only.
// Browser filters and audio execution history are shared or transient.
struct PatchComparisonSound
{
    std::vector<uint8_t> state;
    PresetBrowserState identity;
};
class PatchComparison
{
public:
    int activeSlot() const noexcept { return active; }
    const PatchComparisonSound& other() const noexcept { return alternate; }
    void reset(const ProcessorBridge&,const PresetBrowserState&);
    bool select(int slot,ProcessorBridge&,PresetBrowserState&);
    bool copyFrom(int slot,ProcessorBridge&,PresetBrowserState&);
    // Called only after the complete session has been decoded and validated.
    void recall(int slot,PatchComparisonSound other);
private:
    static PatchComparisonSound capture(const ProcessorBridge&,const PresetBrowserState&);
    bool applyOther(ProcessorBridge&,PresetBrowserState&) const;
    int active=0;
    PatchComparisonSound alternate;
};
}
