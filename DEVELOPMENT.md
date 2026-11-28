# Synth1X development guide

Run the commands below from the repository root. For contribution
and validation practices, see [AGENTS.md](AGENTS.md).

## Build and test

Build the portable synthesis core, tools and tests with CMake 3.25+, Ninja, a
C++20 toolchain, Python 3 and Git:

```sh
cmake --preset core
cmake --build --preset core --parallel 4
ctest --preset core
```

The `core` preset uses `build/core` and fetches GoogleTest for testing.
No plugin SDK, display, audio device, Synth1 DLL or Wine is required.

Build switches can also be set explicitly:

| Configuration | SYNTH1X_BUILD_IPLUG2 | SYNTH1X_BUILD_TOOLS | SYNTH1X_BUILD_TESTS |
| --- | --- | --- | --- |
| Portable library only | OFF | OFF | OFF |
| Offline tools | OFF | ON | OFF |
| Portable tests only | OFF | OFF | ON |
| Native product only | ON | OFF | OFF |

`core` enables tools and tests; native presets disable both. Tests build the Probe
and WAV comparator they require even with optional tools disabled. Use a separate
directory under the Git-ignored `build/` directory for each configuration:

```sh
cmake -S . -B build/tests -DSYNTH1X_BUILD_IPLUG2=OFF -DSYNTH1X_BUILD_TOOLS=OFF -DSYNTH1X_BUILD_TESTS=ON
cmake --build build/tests --parallel 4
ctest --test-dir build/tests --output-on-failure
```

Local builds receive a dated development version. Release builds derive their
version from the Git tag; see [build versions](docs/desktop-build.md#build-versions).

Native products use the pinned `iPlug2/` submodule and additional SDK revisions
in [dependencies.lock.json](dependencies.lock.json). Run
`git submodule sync -- iPlug2` before `git submodule update --init iPlug2`, then follow the
[desktop build guide](docs/desktop-build.md). External `IPLUG2_DIR` checkouts
remain supported; CMake does not fetch or update SDKs. All platforms use the same
pinned development branch of iPlug2; see the build guide for Linux requirements.

[GitHub Actions](docs/ci.md) runs SDK-free portable tests on three platforms and
builds native candidates on Linux/Windows/macOS. Portable test success does not establish
native plugin or DAW operation.

Get development packages from successful [Actions runs](https://github.com/yasuyuki-baba/synth1x/actions).
Extract the downloaded artifact ZIP to find the platform package described in
the [installation guide](INSTALL.md).

## Documentation

| Topic | Guide |
| --- | --- |
| Capabilities and verification limits | [Verification scope](docs/README.md#verification-scope) |
| Product behavior and architecture | [Product contracts](docs/README.md) |
| Portable tests and focused checks | [Testing](tests/README.md) |
| Plugin and standalone builds | [Desktop build guide](docs/desktop-build.md) |
| Editor behavior and layout | [GUI guide](docs/gui/README.md) |
| Rendering, inspection and diagnostics | [Development tools](tools/README.md) |
| Test inputs and evidence | [Evidence](docs/contracts/validation.md) |
| Distribution considerations | [Distribution requirements](docs/publication/README.md) |
