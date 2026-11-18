#pragma once
#include "HistoryQueue.h"
#include "LfoHistoryEvent.h"
namespace synth1x::dsp
{
    template<uint32_t Capacity=4096>
    using LfoHistory=HistoryQueue<LfoHistoryEvent,Capacity>;
}
