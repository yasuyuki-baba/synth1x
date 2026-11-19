#pragma once
#include "patch/PatchMetadata.h"
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace synth1x::host
{
struct LibraryPatch
{
    std::string name;
    std::string text;
    std::string entry;
    // Retain aggregate initialization: source text must be consumed only after
    // the title expression has read it (function argument order is unspecified).
    std::optional<unsigned> color = patchColor(readPatchMetadata(text));

};

// UI-side, immutable-after-open snapshot. No SDK types or paths are needed by
// the audio engine. Archive entries are read in memory, never extracted to disk.
class PatchLibrary
{
public:
    static constexpr std::size_t maximumPatchBytes = 4 * 1024 * 1024;
    static constexpr std::size_t maximumBankBytes = 64 * 1024 * 1024;
    static constexpr std::size_t maximumPatches = 65536;

    bool openFile(const std::string& utf8Path, std::string& error);
    // The catalog fingerprints the same bytes that it passes to the decoder.
    static std::string readSource(const std::filesystem::path& file, std::string& error);
    bool openSource(const std::filesystem::path& file, std::string data, std::string& error);
    const std::vector<LibraryPatch>& patches() const noexcept { return entries; }

private:
    std::vector<LibraryPatch> entries;
};
}
