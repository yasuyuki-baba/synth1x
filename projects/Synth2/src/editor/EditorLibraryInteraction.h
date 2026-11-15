#pragma once
#include "EditorLibraryColumns.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace synth2::editor
{
enum class LibraryRowAction { none, load, favorite, bank, tag };
struct EditorLibraryTarget
{
    LibraryRowAction action=LibraryRowAction::none;
    std::string tag;
};
struct EditorLibraryTag
{
    std::string text;
    float left{},width{};
    bool automatic=false,overflow=false;
};

// Row drawing and mouse input use the same measured, fully visible tag spans.
// Gaps and the overflow marker never name a tag or load a sound.
template<class Measure>
std::vector<EditorLibraryTag> editorLibraryTags(const std::vector<std::string>& manual,
    const std::vector<std::string>& automatic,float viewportWidth,Measure&& measure)
{
    std::vector<EditorLibraryTag> result;
    float left=EditorLibraryColumns::tagsLeft;
    const float right=viewportWidth-26;
    bool full=false;
    const auto append=[&](const std::string& tag,bool inferred) {
        if(full || tag.empty())return;
        if(left>=right){full=true;return;}
        const float width=measure(std::string_view(tag));
        if(left+width+10>right) {
            result.push_back({"...",left,std::min(right-left,static_cast<float>(measure(std::string_view("...")))),inferred,true});
            full=true;return;
        }
        result.push_back({tag,left,width,inferred,false});
        left+=width+10;
    };
    for(const auto& tag:manual)append(tag,false);
    for(const auto& tag:automatic)append(tag,true);
    return result;
}
inline std::optional<std::size_t> editorLibraryRow(float y,float height,double offset,
    std::size_t count,float rowHeight)
{
    if(!std::isfinite(y) || !std::isfinite(height) || !std::isfinite(offset)
        || !std::isfinite(rowHeight) || y<0 || y>=height || offset<0 || rowHeight<=0)
        return std::nullopt;
    const double row=std::floor(y/rowHeight+offset);
    if(row>=static_cast<double>(count))return std::nullopt;
    return static_cast<std::size_t>(row);
}
inline EditorLibraryTarget editorLibraryTarget(float x,float viewportWidth,
    const std::vector<EditorLibraryTag>& tags)
{
    using C=EditorLibraryColumns;
    if(!std::isfinite(x) || x<0 || x>=viewportWidth-18)return {};
    if(x<32)return {LibraryRowAction::favorite,{}};
    if(x<C::nameRight)return {LibraryRowAction::load,{}};
    if(x>=C::bankLeft && x<C::bankRight)return {LibraryRowAction::bank,{}};
    if(x>=C::tagsLeft)
        for(const auto& tag:tags)
            if(!tag.overflow && x>=tag.left && x<tag.left+tag.width)
                return {LibraryRowAction::tag,tag.text};
    return {};
}
inline std::string editorLibraryTagKey(std::string tag)
{
    for(auto& c:tag)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
    return tag;
}
// Shared by the tag menu and inline tag links. This changes filter state only.
inline bool editorToggleLibraryTag(std::vector<std::string>& selection,const std::string& tag)
{
    if(tag.empty())return false;
    const auto key=editorLibraryTagKey(tag);
    const auto found=std::find_if(selection.begin(),selection.end(),[&](const auto& other) {
        return editorLibraryTagKey(other)==key;
    });
    if(found!=selection.end()){selection.erase(found);return true;}
    if(selection.size()>=32)return false;
    selection.push_back(tag);return true;
}
}
