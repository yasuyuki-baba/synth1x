#pragma once
#include "EditorAppearance.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace synth2::editor
{
struct EditorColor
{
    std::uint8_t red=0,green=0,blue=0,alpha=255;
    bool operator==(const EditorColor&) const = default;
    static std::optional<EditorColor> parse(std::string_view text)
    {
        if((text.size()!=7 && text.size()!=9) || text[0]!='#')return {};
        std::uint32_t value=0;
        for(char c:text.substr(1)) {
            int digit=c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : c>='A' && c<='F' ? c-'A'+10 : -1;
            if(digit<0)return {};
            value=(value<<4)|static_cast<unsigned>(digit);
        }
        if(text.size()==7)value=(value<<8)|255;
        return EditorColor{static_cast<std::uint8_t>(value>>24),static_cast<std::uint8_t>(value>>16),
            static_cast<std::uint8_t>(value>>8),static_cast<std::uint8_t>(value)};
    }
    std::string hex() const
    {
        constexpr char digits[]="0123456789abcdef";
        std::string result="#";
        for(auto channel:{red,green,blue,alpha}) {result+=digits[channel>>4];result+=digits[channel&15];}
        return result;
    }
};
enum class PaletteRole
{
    background,header,headerEdge,panel,groupPanel,groupText,border,text,muted,accent,
    display,displayText,selectedRow,control,controlEdge,track,pointer,heading,keyboardWhite,count
};
inline constexpr std::size_t paletteRoleCount=static_cast<std::size_t>(PaletteRole::count);
inline constexpr std::array<std::string_view,paletteRoleCount> paletteRoleKeys {
    "background","header","headerEdge","panel","groupPanel","groupText","border","text","muted","accent",
    "display","displayText","selectedRow","control","controlEdge","track","pointer","heading","keyboardWhite"};
inline constexpr std::array<const char*,paletteRoleCount> paletteRoleLabels {
    "Background","Header","Header edge","Panel","Inner panel","Inner heading","Border","Text","Secondary text","Accent",
    "Display","Display text","Selection","Control","Control edge","Track","Pointer","Group heading","White keys"};
// Stable storage keys, independent of panel order and translated labels.
inline constexpr std::array<std::string_view,13> paletteGroupKeys {
    "oscillators","filter","amplifier","lfo1","lfo2","voice","unison","arpeggiator","effect","chorus","delay","eq","midi"};
inline constexpr std::array<const char*,13> paletteGroupLabels {
    "OSCILLATORS","FILTER","AMPLIFIER","LFO 1","LFO 2","VOICE","UNISON","ARPEGGIATOR","EFFECT","CHORUS","DELAY","EQ","MIDI"};
inline int paletteGroup(std::string_view title)
{
    for(std::size_t i=0;i<paletteGroupLabels.size();++i)if(title==paletteGroupLabels[i])return static_cast<int>(i);
    return -1;
}
inline bool paletteGroupRole(PaletteRole role)
{
    return role==PaletteRole::panel || role==PaletteRole::groupPanel || role==PaletteRole::groupText ||
        role==PaletteRole::border || role==PaletteRole::text || role==PaletteRole::muted || role==PaletteRole::accent ||
        role==PaletteRole::control || role==PaletteRole::controlEdge || role==PaletteRole::track ||
        role==PaletteRole::pointer || role==PaletteRole::heading;
}
struct EditorPalette
{
    using Colors=std::array<EditorColor,paletteRoleCount>;
    Colors global;
    std::array<std::array<std::optional<EditorColor>,paletteRoleCount>,paletteGroupKeys.size()> groups{};
    EditorPalette()
    {
        const auto& a=editorAppearance();
        const std::array<const char*,paletteRoleCount> defaults {
            a.background,a.header,a.headerEdge,a.panel,a.groupPanel,a.groupText,a.border,a.text,a.muted,a.accent,
            a.display,a.displayText,a.selectedRow,a.control,a.controlEdge,a.track,a.pointer,a.groupText,"#dad7cd"};
        for(std::size_t i=0;i<global.size();++i)global[i]=*EditorColor::parse(defaults[i]);
        groups[1][static_cast<std::size_t>(PaletteRole::panel)]=*EditorColor::parse(a.filterPanel);
        groups[1][static_cast<std::size_t>(PaletteRole::heading)]=*EditorColor::parse(a.accent);
    }
    EditorColor resolve(PaletteRole role,int group=-1) const
    {
        const auto index=static_cast<std::size_t>(role);
        if(group>=0 && group<static_cast<int>(groups.size()) && groups[group][index])return *groups[group][index];
        return global[index];
    }
    bool set(PaletteRole role,EditorColor value,int group=-1)
    {
        const auto index=static_cast<std::size_t>(role);
        if(index>=paletteRoleCount || group < -1 || group>=static_cast<int>(groups.size()))return false;
        if(group>=0) {
            if(!paletteGroupRole(role))return false;
            groups[group][index]=value;
        } else {
            // The canvas is always opaque; alpha applies to the layers above it.
            if(role==PaletteRole::background)value.alpha=255;
            global[index]=value;
        }
        return true;
    }
    void inherit(int group) {if(group>=0 && group<static_cast<int>(groups.size()))groups[group]={};}
    bool operator==(const EditorPalette&) const = default;
};
class PaletteOwner
{
public:
    virtual ~PaletteOwner()=default;
    virtual const EditorPalette& currentPalette() const=0;
};
// Preview belongs to one editor. Applying is separate from persistence so a
// failed write leaves the edit open and cancellable.
struct EditorPaletteEdit
{
    EditorPalette original;
    bool active=false;
    void begin(const EditorPalette& palette) {if(!active){original=palette;active=true;}}
    bool modified(const EditorPalette& palette) const {return active && palette!=original;}
    void cancel(EditorPalette& palette) {if(active){palette=original;active=false;}}
    void apply() {active=false;}
};
}
