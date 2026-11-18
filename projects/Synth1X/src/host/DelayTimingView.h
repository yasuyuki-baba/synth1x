#pragma once

namespace synth1x::host
{
// Approximate read-only audio-thread telemetry for the editor, not DSP controls.
struct DelayTimingView
{
    float bpm {},leftMilliseconds {},rightMilliseconds {};
};
}
