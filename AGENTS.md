# Working on Synth1X

## Communication and project context

- Write code, documentation and commit messages in English.
- Use ASCII hyphens (`-`) for em-dash punctuation in prose.
- Name new branches with a prefix that reflects the primary purpose of the
  change and a short lowercase, hyphenated topic. Use `feature/` for new
  functionality, `fix/` for bug fixes, `docs/` for documentation changes, and
  `chore/` for maintenance. Examples include `feature/migration-scripts`,
  `fix/wine-capture`, `docs/upgrade-guide`, and `chore/rename-to-synth1x`.
- Read the relevant [product contract](docs/README.md) and independent evidence
  before changing compatibility behavior. Update a requirement only for a product
  decision or new independent evidence.
- Validate locally and let normal pushes run CI. Use a skip directive only when
  the owner explicitly requests pausing Actions for the current work.

## Product and architecture

- Keep product code in `projects/Synth1X`, the portable core independent of SDK
  headers, and JUCE out of the build graph. Pin the iPlug2 submodule and lock
  entry together. Support external `IPLUG2_DIR` overrides, target-scoped compiler
  requirements and SDK-free builds. Keep offline tools independently selectable.
  CMake must not fetch native SDKs. Follow the [desktop build guide](docs/desktop-build.md)
  for platform targets, arm64-only macOS releases and tag-derived version metadata.
- Use PascalCase for portable C++ files and types. Use Settings, Controls, Input,
  State or View according to the role. Avoid Params, Parameters and Config type
  suffixes. Reserve Synth1-prefixed names for source loading and conversion.
  Run `python3 tools/lint_naming.py` when changing these names.
- Keep source-version decisions in the [loading adapter](docs/architecture/source-conversion.md).
  The [render boundaries](docs/architecture/render-boundaries.md) define the
  controls DSP components can consume. Preserve ownership across
  [voice lifetime](docs/architecture/voice-lifecycle.md),
  [tuning and dispatch](docs/architecture/voice-orchestration.md),
  [voice DSP](docs/architecture/voice-audio-components.md),
  [shared modulation](docs/architecture/shared-modulation.md) and
  [output processing](docs/architecture/output-pipeline.md).
- Keep portable presentation independent of host and engine headers. For editor
  changes follow the [GUI contract](docs/gui/README.md),
  [library behavior](docs/gui/patch-library.md) and [A/B ownership](docs/gui/patch-comparison.md).
  Preserve dialog lifetime guards when splitting native UI code. Use the framework
  keyboard and MIDI delivery. Keep bundled fonts, one default palette and an empty
  initial library.

## Validation

- Use the [test guide](tests/README.md) for focused checks while developing.
  Once changes settle, build affected targets and run the applicable suite.
  Portable tests use the actual bridge and self-authored inputs and run without
  credentials, another checkout, original recordings, Synth1 or Wine.
- For DSP or loading refactors, compare preceding and candidate WAV bytes and
  resolved controls on the same inputs. Preserve independent expectations and the
  exact MIDI timeline, including equal timestamps and sub-block offsets. Investigate
  loading as well as synthesis when a result differs. Production decisions must
  never depend on patch numbers, titles, bank identities or source hashes.
  See the [comparison procedure](docs/validation/reference-comparisons.md).
- Treat changes in arithmetic order, receiver timing, RNG advancement or state
  lifetime as behavior changes. For precision work, measure CPU cost and raw stereo
  error and apply the [audio acceptance rules](docs/contracts/validation.md#precision-audio-acceptance).
  Keep allocation, blocking locks and I/O out of audio callbacks.
- For shared synthesis changes, build the Probe, LoadingProbe, tests and affected
  native formats/APP where the toolchain is available. After changing component
  boundaries, run `python3 tools/audit_dsp_headers.py --build-dir build-oos-core`.
  Report the checks actually run and their scope. Use the [manual platform
  guide](docs/validation/manual-platform.md) for GUI, DAW and device checks.

## Documentation and distribution

- Keep durable working instructions here, product requirements in the
  [contracts](docs/README.md), and the [verification map](docs/validation/coverage.md)
  current when coverage changes. Link to the detailed source. Keep experiment
  journals, machine-specific paths and dated pass counts out of product requirements.
- Keep product specifications, build instructions and tests self-contained.
  Keep companion investigation workflows in their own repository.
- Follow the [distribution requirements](docs/publication/README.md) for licensing,
  asset rights, attribution and notices. Record unresolved permissions. Treat
  rewriting Git history or changing repository visibility as separate publication
  decisions.
