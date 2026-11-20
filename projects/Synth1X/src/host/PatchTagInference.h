#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace synth1x::host
{
// Bump when inference rules or resolved loading semantics change.
inline constexpr int patchTagCacheVersion=3;
struct PatchTagReason
{
    std::string rule, detail;
    bool operator==(const PatchTagReason&) const = default;
};
struct PatchTagSuggestion
{
    std::string tag;
    std::vector<PatchTagReason> reasons;
    bool operator==(const PatchTagSuggestion&) const = default;
};
// Inputs are resolved host dials, never raw version-dependent source entries.
// Heuristic suggestions describe names/settings, not measured audio semantics.
std::vector<PatchTagSuggestion> explainPatchTags(std::string_view name,const std::array<float,129>& dials);
}
