# Public verification map

Use this index to find the automated check for a product change. The
[test guide](../../tests/README.md) gives build commands, focused runs and
sanitizer options. The [manual platform guide](manual-platform.md) covers GUI
interaction, DAW sessions and physical devices.

## Loading, state and editor

| Area | Representative tests | What they verify |
| --- | --- | --- |
| Patch import and export | [PatchFormat](../../tests/PatchFormatTest.cpp), [PatchImportWorkspace](../../tests/PatchImportWorkspaceTest.cpp), [LoadingBehavior](../../tests/LoadingBehaviorTest.cpp) | Version conversion, source metadata, fractional dials and rejected input. Accepted file versions and independently observed native versions are listed in the [loading contract](../contracts/patch-loading.md). |
| Banks and library | [ArchiveBank](../../tests/ArchiveBankTest.cpp), [BankBoundary](../../tests/BankBoundaryTest.cpp), [PresetFiles](../../tests/PresetFilesTest.cpp), [PatchCatalog](../../tests/PatchCatalogTest.cpp), [tag reports](../../tests/test_tag_reports.py) | Archive limits, save/refresh, source identity, shared caches, filters, tag overrides and Program Change slots. Tag tests exercise authored examples. |
| Host parameters and session state | [ProcessorContracts](../../tests/ProcessorContractsTest.cpp), [ProcessorState](../../tests/ProcessorStateTest.cpp), [StateRepresentation](../../tests/StateRepresentationTest.cpp), [PatchComparison](../../tests/PatchComparisonTest.cpp) | Parameter definitions, numeric parsing, transactional recall and A/B sound ownership. |
| Editor presentation and input | [EditorLayout](../../tests/EditorLayoutTest.cpp), [EditorInteraction](../../tests/EditorInteractionTest.cpp), [EditorKeyboard](../../tests/EditorKeyboardTest.cpp), [EditorPalette](../../tests/EditorPaletteTest.cpp) | Layout bounds, effect labels, control binding, keyboard mapping and palette edit/import/apply behavior. Native drawing, focus and dialogs require a running editor. |

## DSP and receiver coverage

| Area | Representative tests | What they verify |
| --- | --- | --- |
| MIDI, voices and ARP | [PerformanceBehavior](../../tests/PerformanceBehaviorTest.cpp), [HostLifecycle](../../tests/HostLifecycleTest.cpp), [ArpeggiatorBehavior](../../tests/ArpeggiatorBehaviorTest.cpp), [GeneratedInput](../../tests/GeneratedInputTest.cpp) | Event order, controller routing, note admission, pedal, core reuse, release and buffer bounds for exercised schedules. |
| Shared modulation and history | [ModulationBehavior](../../tests/ModulationBehaviorTest.cpp), [LfoPhasePrecision](../../tests/LfoPhasePrecisionTest.cpp), [HistoryQueueBehavior](../../tests/HistoryQueueBehaviorTest.cpp) | Clock and random-source ownership, phase progression, receiver ordering and queue overflow/reuse. |
| Oscillators and render precision | [OscillatorRead](../../tests/OscillatorReadTest.cpp), [FmPhaseCorrection](../../tests/FmPhaseCorrectionTest.cpp), [PwmPrecision](../../tests/PwmPrecisionTest.cpp), [Oversampling](../../tests/OversamplingTest.cpp), [BoundedPrecision](../../tests/BoundedPrecisionTest.cpp) | Phase, interpolation, FM/Sync/Ring, quality switches, latency and folded-harmonic checks on generated signals. |
| Envelopes and filters | [ContinuousLevel](../../tests/ContinuousLevelTest.cpp), [FilterEnvelopePrecision](../../tests/FilterEnvelopePrecisionTest.cpp), [FilterPath](../../tests/FilterPathTest.cpp), [FilterBehavior](../../tests/FilterBehaviorTest.cpp) | Fractional edits, envelope continuity, coefficient changes and retained kernel state. |
| Output and effects | [OutputPipelinePath](../../tests/OutputPipelinePathTest.cpp), [EqualizerBehavior](../../tests/EqualizerBehaviorTest.cpp), [EffectLifecycle](../../tests/EffectLifecycleTest.cpp), [DelayPath](../../tests/DelayPathTest.cpp), [ChorusPath](../../tests/ChorusPathTest.cpp) | Stage order, stereo routing, tails, reset, delay timing and chorus interpolation. |
| Instance and callback behavior | [PlaybackBehavior](../../tests/PlaybackBehaviorTest.cpp), [RealtimeBehavior](../../tests/RealtimeBehaviorTest.cpp) | Separate-instance isolation, prepared callback heap activity and allocation-failure ownership. Heap instrumentation covers C++ allocation and, on Linux, wrapped C heap calls. Timing and device underruns require separate measurement. |

Generated inputs exercise particular signal paths and event histories. For a
claim about original-engine compatibility or preservation across a refactor,
use the [validation contract](../contracts/validation.md) and the
[reference comparison procedure](reference-comparisons.md).

## Build, tools and native formats

| Area | Checks | Scope |
| --- | --- | --- |
| Portable build and tooling | [CTest registrations](../../cmake/PortableTests.cmake) | Naming, header dependencies on supported compilers, gain and WAV diagnostics, probe isolation, version metadata and package preparation. |
| Native VST3 host API | [PatchNotificationTest](../../tests/sdk/vst3/PatchNotificationTest.cpp) | Windows/Linux headless checks of parameter publication, host echo, MIDI offsets, Program Change and A/B recall. |
| Built-plugin audio | [Native smoke tests](../../tests/native/README.md) | Loads actual VST3/CLAP modules and macOS AUv2 through host APIs. Checks sound, silence and state restore without a DAW or audio device. |

Record the platform and configuration when reporting a native or sanitizer run.
