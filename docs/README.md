# Synth1X product documentation

| Need | Maintained contract |
| --- | --- |
| Source formats, defaults and transactional loading | [Patch loading](contracts/patch-loading.md) |
| Stable parameters, saved state and callback interface | [Processor/state](contracts/processor-state.md) |
| MIDI, ARP and physical/logical voices | [MIDI and voices](contracts/midi-voices.md) |
| Supported MIDI messages and controllers | [MIDI Implementation Chart](midi-implementation.md) |
| Shared clocks, random streams and input history | [Modulation](contracts/modulation.md) |
| DSP/output order, numerical behavior and silence | [Audio rendering](contracts/audio-rendering.md) |
| Test oracles, provenance and compatibility scope | [Validation](contracts/validation.md) |
| Source-version strategy boundary | [Source conversion](architecture/source-conversion.md) |
| Component inputs and state ownership | [Render boundaries](architecture/render-boundaries.md) |
| Voice audio component inputs and state | [Voice audio components](architecture/voice-audio-components.md) |
| Output order and lifecycle retention | [Output pipeline](architecture/output-pipeline.md) |
| Offline callback timing | [Host benchmark](../tools/README.md#offline-host-benchmark) |
| Automated verification scope | [Verification map](validation/coverage.md) |
| Manual platform checks | [Platform guide](validation/manual-platform.md) |
| Before/after audio and loading comparisons | [Reference comparisons](validation/reference-comparisons.md) |
| CI checks and candidate artifacts | [Continuous integration](ci.md) |
| Native formats and toolchains | [Desktop build boundary](desktop-build.md) |
| Library archives, tags and recall | [Patch library](gui/patch-library.md) |
| Editor behavior and design | [GUI](gui/README.md) |

## Verification scope

Portable tests exercise the core with self-authored inputs. Headless native tests
load built plugins and check host API behavior. The [verification map](validation/coverage.md)
identifies what each suite covers. Use the [manual platform guide](validation/manual-platform.md)
for editor interaction, DAW sessions and physical audio/MIDI devices.

Original-instrument comparisons apply to the measured patches, pitches and
render settings. See the [results and conditions](validation/reference-comparisons.md#rydeen-bank-comparison).
For upgrades, the [migration guide](../MIGRATION.md) explains how changed plugin
IDs affect existing DAW sessions.

Use the [test guide](../tests/README.md) to run checks and the
[distribution requirements](publication/README.md) when preparing packages.
