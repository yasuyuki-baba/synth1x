#include "Synth1X.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include <cmath>

using namespace iplug;

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
        if (!queue)
            return nullptr;

        const auto id = queue->getParameterId();
        if (id == kPresetParam) {
            // Program Change can arrive in a parameter-only callback.
            const int last = queue->getPointCount() - 1;
            int32 offset;
            double value;
            if (last >= 0 && readPoint(*queue, last, offset, value)) {
                const auto program = static_cast<int>(std::lround(
                    value * (synth1x::host::PatchCatalog::programCount - 1)));
                IMidiMsg message;
                message.MakeProgramChange(program, 0, offset);
                plugin.ProcessMidiMsg(message);
            }
            return nullptr;
        }
        if (!isMidiControllerParameter(id))
            return queue;

        const int midiIndex = id - kMIDICCParamStartIdx;
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
    auto* changes = data.inputParameterChanges;
    if (!changes)
        return Plugin::process(data);

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
