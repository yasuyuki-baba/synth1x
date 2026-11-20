#pragma once
#include "ProcessorBridge.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace synth1x::host
{
// Instance-owned state; paths are relative to the independently configured root.
struct PresetBrowserState
{
    std::string query, selectedId, patchName = "Init", bankId, tag;
    bool favoritesOnly = false;
    bool restricted = false;
    std::vector<std::string> importedIds;
    std::vector<std::string> selectedTags;
    int scroll = 0, page = 0;
    std::array<double, ProcessorBridge::parameterCount> baseline{};
    void capture(const ProcessorBridge &);
    bool modified(const ProcessorBridge &) const;
    std::string serialize() const;
    static bool deserialize(const std::string &, PresetBrowserState &);
};
class PatchComparison;
std::vector<uint8_t> savePresetSession(const ProcessorBridge &, const PresetBrowserState &,
                                       const PatchComparison* comparison=nullptr);
// Accepts wrapper v1/v2 and A/B v3; validates both slots before applying either.
int restorePresetSession(const void *, int size, ProcessorBridge &, PresetBrowserState &,
                         PatchComparison* comparison=nullptr);
} // namespace synth1x::host
