#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace synth1x::editor
{
enum class LibraryFilterKind { query, bank, tag, favorites, registration };
struct EditorLibraryFilter
{
    LibraryFilterKind kind;
    std::string value,label;
    bool operator==(const EditorLibraryFilter&) const = default;
};
struct EditorLibraryFilterChip
{
    std::size_t index;
    float left,width;
    std::size_t remaining=0;
};
inline std::string editorLibraryMoreFilters(std::size_t count)
{
    return "+"+std::to_string(count)+" more";
}
// Reserve a measured overflow button so every hidden condition remains reachable.
// Drawing and input share these spans; gaps never remove a filter.
template<class Measure>
std::vector<EditorLibraryFilterChip> editorLibraryFilterChips(
    const std::vector<EditorLibraryFilter>& filters,float width,Measure&& measure)
{
    std::vector<EditorLibraryFilterChip> result;
    if(!std::isfinite(width) || width<=0)return result;
    constexpr float gap=6;
    std::vector<float> widths;
    float total=0;
    for(const auto& filter:filters) {
        const float chipWidth=std::min(240.f,static_cast<float>(measure(filter.label))+30.f);
        widths.push_back(chipWidth);total+=chipWidth;
    }
    if(!filters.empty())total+=gap*(filters.size()-1);
    const bool allFit=total<=width;
    float left=0;
    for(std::size_t i=0;i<filters.size();++i)
    {
        const float chipWidth=widths[i];
        const auto remaining=filters.size()-i-1;
        const float moreWidth=remaining ? static_cast<float>(measure(editorLibraryMoreFilters(remaining)))+20.f : 0.f;
        if(!allFit && left+chipWidth+(remaining ? gap+moreWidth : 0.f)>width)
        {
            result.push_back({i,left,std::min(width-left,
                static_cast<float>(measure(editorLibraryMoreFilters(remaining+1)))+20.f),remaining+1});
            break;
        }
        result.push_back({i,left,chipWidth});
        left+=chipWidth+gap;
    }
    return result;
}
inline const EditorLibraryFilterChip* editorLibraryFilterChipAt(
    float x,const std::vector<EditorLibraryFilterChip>& chips)
{
    if(!std::isfinite(x))return nullptr;
    for(const auto& chip:chips)
        if(x>=chip.left && x<chip.left+chip.width)return &chip;
    return nullptr;
}
}
