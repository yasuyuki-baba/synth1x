# Running product tests

Start with the [portable build and test commands](../DEVELOPMENT.md#build-and-test).
For an offline configure, supply a local GoogleTest checkout with
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest`.
Run the commands below from the repository root.

## Focused and instrumented runs

The [verification map](../docs/validation/coverage.md) identifies individual test suites
and their scope. Select the relevant CTest entry or GoogleTest suite, for example:

```sh
ctest --test-dir build/core -N
ctest --test-dir build/core --output-on-failure -R '^BuildToolsTests$'
./build/core/Synth1XPatchTests --gtest_filter='HostLifecycle.*'
```

Use `Synth1XPatchTests.exe` on Windows. These examples run subsets, not the complete
suite; retain the executed filter when recording results.

For an additional local memory/undefined-behavior check with GCC or Clang:

```sh
cmake -S . -B build/sanitized -DSYNTH1X_BUILD_IPLUG2=OFF \
  -DXZ_SANDBOX=no \
  -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build/sanitized --target Synth1XPatchTests Synth1XRealtimeTests --parallel 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ./build/sanitized/Synth1XPatchTests
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ./build/sanitized/Synth1XRealtimeTests
```

The xz command-line sandbox must be disabled for sanitizer builds; this does not
change the library's archive limits or enable additional archive formats.

LeakSanitizer requires an environment without ptrace attachment.

## SDK parameter check

An optional check uses the actual pinned iPlug2 parameter class to verify integer
versus continuous host registration, sparse choice names, full source ranges and
MIDI source words. It needs the public `iPlug2` submodule but no plugin SDK, GUI or
audio device. On Linux with a C++20 compiler, run from the repository root:

```sh
mkdir -p build/parameter-sdk
c++ -std=c++20 -DNDEBUG -I iPlug2/IPlug -I iPlug2/WDL -I projects/Synth1X \
  tests/sdk/ParameterSetupTest.cpp iPlug2/IPlug/IPlugParameter.cpp -pthread \
  -o build/parameter-sdk/ParameterSetupTest
build/parameter-sdk/ParameterSetupTest
```

## VST3 patch host check

This headless check builds the production wrapper against the pinned iPlug2 and
VST3 sources on Windows and Linux. Initialize the iPlug2 submodule, then run:

```sh
python3 tools/prepare_native_sdks.py --output build/dependencies.json
cmake -S tests/sdk/vst3 -B build/vst3-test -DCMAKE_BUILD_TYPE=Release
cmake --build build/vst3-test --config Release --parallel 4
ctest --test-dir build/vst3-test -C Release --output-on-failure
```

## Built-plugin audio smoke

Follow the [native smoke test instructions](native/README.md) to exercise the
built VST3, CLAP and macOS AUv2 modules.

## Callback heap checks

Run the isolated heap instrumentation in its own test executable:

```sh
ctest --preset core -L realtime
```

## Archive fixtures

The archive tests run against checked-in fixtures without an encoder. To
regenerate them, follow the [RAR](fixtures/rar/README.md) or
[7z](fixtures/7z/README.md) instructions.
