# Product development tools

These tools use the portable core and do not require an original Synth1 DLL or
compatibility checkout. Native APP capture is an optional Wine GUI workflow.

| Task | Entry point |
| --- | --- |
| Build and run product contracts | [tests/README.md](../tests/README.md) |
| Render patches and exact MIDI histories | [Synth2Probe](synth2_probe.cpp) |
| Inspect resolved controls and selected DSP routes | [Synth2LoadingProbe](synth2_loading_probe.cpp) |
| Explain automatic tags and compare rule/loading changes | [tag_patches.py](tag_patches.py), backed by Synth2TagProbe |
| Compare WAVs for diagnostics | [CompareWaves](compare_waves.cpp) |
| Audit DSP, voice, editor and state header compilation/dependency boundaries | audit_dsp_headers.py; GNU-style GNU/Clang Ninja/Makefile builds also provide CTest `HeaderBoundaryAudit` and target `Synth2HeaderAudit` |
| Check portable C++ file/type names and setting suffixes | `python3 tools/lint_naming.py` or CMake target `Synth2NamingLint` |
| Preview/render the editor study | preview_editor.py, render_editor_study.py |
| Capture the Windows APP editor | capture_iplug_wine.py |

Use the [reference comparison procedure](../docs/validation/reference-comparisons.md) for a
self-authored input example, exact MIDI timeline format, loading-probe file layout
and byte-for-byte before/after checks. The loading probe is a case-based tool, not
a single-patch command; its mode determines which values it reports.

Editor generation is described in the [GUI guide](../docs/gui/README.md); native
APP capture is covered by the [platform guide](../docs/validation/manual-platform.md#native-windows-app-capture).

`measurement_common.py` supplies standard-library WAV decoding, hashes, raw stereo
errors and logged subprocess I/O. Its tests are part of the portable suite.
Aligned/downmixed/gain-fitted diagnostics from CompareWaves cannot replace raw
stereo compatibility evidence.

Configure with `SYNTH2_BUILD_TOOLS=ON` to build the offline tool targets; the
portable `core` preset enables them. Native presets disable tools. Tests enabled
with `SYNTH2_BUILD_TOOLS=OFF` still build the required Probe and WAV comparator,
but omit LoadingProbe, the editor exporter, the host benchmark and the tag probe.

## Automatic tag reports

`tag_patches.py` runs the product's portable loading and inference code without
starting a plugin or rendering audio. It accepts a `.sy1`, ZIP/RAR/7z bank or a
recursive directory, and writes JSON with stable relative source IDs, exact source
SHA-256 hashes, load errors, suggested tags and their reasons. Name words/categories
and settings have distinct rule identifiers. Reports describe heuristic suggestions,
not measured sound classification or an accuracy score. Manual tags and hidden-tag
overrides are not applied to these diagnostic suggestions.

```sh
cmake --preset core
cmake --build build-oos-core --target Synth2TagProbe
python3 tools/tag_patches.py scan path/to/patches --output before.json
# After changing and rebuilding the inference/loading implementation:
python3 tools/tag_patches.py scan path/to/patches --output after.json
python3 tools/tag_patches.py compare before.json after.json --output changes.json
```

Use `--probe path/to/Synth2TagProbe` for another build; the Windows default selects
`Synth2TagProbe.exe`. A single patch works with the same `scan` command. Omit
`--output` to write UTF-8 JSON to stdout. Existing output files are preserved;
choose a fresh path for each run. Scans do not register sounds, write caches or
modify source files. Reports contain hashes rather than copies of patch source
text. Invalid legacy title bytes become replacement characters in display text;
the source hash still covers the original bytes.

Comparison separates added/removed sources, edited source contents, tag additions
and removals, and changes to reasons for retained tags. Totals for unchanged-source
tag/reason changes exclude edited files. Source identities must be unique and
report schema versions must be supported. Keep the same source layout between
captures; changing the input root or switching between loose and archived sources
can change identities. Compare reports against independently reviewed examples;
increased tag coverage by itself does not establish better classification.

`scan` returns 0 when all sources load, 2 when the report includes per-patch load
errors, and 1 for a tool/input/output failure. `compare` returns 0 after producing
a report even when differences exist. The lower-level `Synth2TagProbe` streams
records to the Python wrapper and is built only when optional tools are enabled.

Native CI helpers: `prepare_native_sdks.py` installs lock-file SDK revisions and
writes a build-identity manifest. `package_native.py` checks the expected native
products and creates a Windows ZIP or macOS tar.gz with metadata and notices.
See [CI](../docs/ci.md) for local commands and verification boundaries.

## Offline host benchmark

`Synth2HostBenchmark` is a non-gating offline diagnostic. It exercises explicitly
specified dry/wet synthetic profiles, checks core occupancy and audio, and reports
callback timing separately from setup and validation work. Use fresh CSV outputs,
record compiler/binary/CPU/run context and compare like-for-like conditions.

```sh
cmake --preset core
cmake --build build-oos-core --target Synth2HostBenchmark
./build-oos-core/Synth2HostBenchmark build-oos-core/host-benchmark.csv 128
```

On Windows, invoke `build-oos-core/Synth2HostBenchmark.exe` instead. The CSV path
must not exist; choose a fresh filename for each run.

Offline over-budget observations are not detected audio-device underruns. Neither
average nor sampled maximum callback time establishes a hard real-time deadline.
Actual DAW/device testing and concurrency contracts require separate validation.

`Synth2Probe --oversampling FACTOR` selects 1, 2, 4, 8 or 16 before preparation.
The default is 1. Higher-factor WAVs include processing latency; do not silently
align or gain-fit them when reporting raw audio comparisons.

## Isolated rendering diagnostics

Normal product targets and `Synth2Probe` do not read DSP bypass environment
variables. To isolate envelopes, filters or output effects, explicitly build
`cmake --build build-oos-core --target Synth2DiagnosticProbe`. This optional target
links a separate diagnostic core and is excluded from normal builds. Invoke it
with `--bypass-env`, `--bypass-filter` or `--bypass-fx`; the normal probe rejects
these switches. Bypass state belongs to the diagnostic rendering thread, is set
outside processing callbacks, and is neither a patch control nor saved processor state.
`--bypass-env` retains the probe's accompanying isolation dial overrides.

`Synth2RenderProfile` profiles generated bridge playback scenarios without a
pass/fail timing bound. See [host validation](../docs/validation/manual-platform.md) for the
command and interpretation of its per-case CSV and final-block hashes.

`SYNTH2_PROBE_CONTROLLER_TRACE` writes one tab-separated numeric row per processed
callback: end frame; bend current, target, moving and offset; cutoff index; FM
amount; amplitude gain; LFO1 depth; LFO2 depth. Rows have ten columns. The retired
receiver-admission flag is omitted; the first five pitch-history columns retain
their order. Historical eleven-column captures contain that old flag at index5.

## Float32 calibration constants

Run `python3 tools/generate_oscillator_constants.py` to regenerate the tracked
waveform, MIDI/pitch, scaled-exponential and control/phase-word headers under
`projects/Synth2/src/dsp`. The generator evaluates the source-defined calibration
formulas offline; the renderer uses Float32 words and integer phase/frame metadata.
No runtime double renderer or precision-mode selection is involved. Review formula
changes independently from generated data, and validate audio/timing compatibility
with the existing error policy.
