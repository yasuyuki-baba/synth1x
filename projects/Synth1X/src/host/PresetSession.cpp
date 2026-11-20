#include "PresetSession.h"
#include "PatchComparison.h"
#include "ProcessorState.h"
#include <tinyxml2.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>
#include <utility>

namespace synth1x::host
{
void PresetBrowserState::capture(const ProcessorBridge &p)
{
    for (int i = 0; i < p.parameterCount; ++i)
        baseline[i] = p.normalizedValue(i);
}
bool PresetBrowserState::modified(const ProcessorBridge &p) const
{
    for (int i = 0; i < p.parameterCount; ++i)
        if (std::abs(baseline[i] - p.normalizedValue(i)) > 1e-7)
            return true;
    return false;
}
std::string PresetBrowserState::serialize() const
{
    tinyxml2::XMLDocument d;
    auto *e = d.NewElement("browser");
    d.InsertEndChild(e);
    e->SetAttribute("version", 1);
    e->SetAttribute("query", query.c_str());
    e->SetAttribute("bank", bankId.c_str());
    e->SetAttribute("tag", selectedTags.empty() ? tag.c_str() : selectedTags.front().c_str());
    for(const auto& selected:selectedTags) {
        auto* child=d.NewElement("tag");child->SetAttribute("name",selected.c_str());e->InsertEndChild(child);
    }
    e->SetAttribute("favorites", favoritesOnly);
    e->SetAttribute("selected", selectedId.c_str());
    e->SetAttribute("name", patchName.c_str());
    e->SetAttribute("restricted", restricted);
    e->SetAttribute("scroll", scroll);
    e->SetAttribute("page", page);
    for (const auto &id : importedIds)
    {
        auto *child = d.NewElement("item");
        child->SetAttribute("id", id.c_str());
        e->InsertEndChild(child);
    }
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(17);
    for (double v : baseline)
        s << v << ' ';
    auto *b = d.NewElement("baseline");
    b->SetText(s.str().c_str());
    e->InsertEndChild(b);
    tinyxml2::XMLPrinter p;
    d.Print(&p);
    return p.CStr();
}
bool PresetBrowserState::deserialize(const std::string &text, PresetBrowserState &target)
{
    if (text.size() > 32 * 1024 * 1024)
        return false;
    tinyxml2::XMLDocument d;
    if (d.Parse(text.data(), text.size()) != tinyxml2::XML_SUCCESS)
        return false;
    auto *e = d.RootElement();
    if (!e || std::string(e->Name()) != "browser" || e->IntAttribute("version") != 1)
        return false;
    PresetBrowserState next;
    auto attr = [&](const char *name)
    {
        auto *s = e->Attribute(name);
        return s ? std::string(s) : std::string{};
    };
    next.query = attr("query");
    next.bankId = attr("bank");
    next.tag = attr("tag");
    for(auto* child=e->FirstChildElement("tag");child;child=child->NextSiblingElement("tag")) {
        const auto* name=child->Attribute("name");
        if(!name || !*name || std::string_view(name).size()>128 || next.selectedTags.size()>=32)return false;
        next.selectedTags.emplace_back(name);
    }
    next.favoritesOnly = e->BoolAttribute("favorites");
    next.selectedId = attr("selected");
    next.patchName = attr("name");
    next.restricted = e->BoolAttribute("restricted");
    next.scroll = std::max(0, e->IntAttribute("scroll"));
    next.page = std::clamp(e->IntAttribute("page"), 0, 2);
    for (auto *c = e->FirstChildElement("item"); c; c = c->NextSiblingElement("item"))
    {
        auto *id = c->Attribute("id");
        if (!id)
            return false;
        next.importedIds.emplace_back(id);
    }
    auto *b = e->FirstChildElement("baseline");
    if (!b || !b->GetText())
        return false;
    std::istringstream values(b->GetText());
    values.imbue(std::locale::classic());
    for (auto &v : next.baseline)
        if (!(values >> v) || !std::isfinite(v) || v < 0 || v > 1)
            return false;
    values >> std::ws;
    if (!values.eof())
        return false;
    target = std::move(next);
    return true;
}
namespace
{
void putSessionWord(std::vector<uint8_t>& bytes,uint32_t value)
{
    for(int i=0;i<4;++i)bytes.push_back(static_cast<uint8_t>(value>>(i*8)));
}
std::vector<uint8_t> encodeSoundSession(const std::vector<uint8_t>& sound,const PresetBrowserState& browser)
{
    const auto view=browser.serialize();
    std::vector<uint8_t> bytes;
    putSessionWord(bytes,0x32504953);putSessionWord(bytes,2);
    putSessionWord(bytes,static_cast<uint32_t>(sound.size()));
    bytes.insert(bytes.end(),sound.begin(),sound.end());
    putSessionWord(bytes,static_cast<uint32_t>(view.size()));
    bytes.insert(bytes.end(),view.begin(),view.end());return bytes;
}
struct DecodedSoundSession
{
    int size=0,version=0;
    PatchComparisonSound sound;
};
bool decodeSoundSession(const void* data,int size,
    const std::array<ParameterInfo,ProcessorBridge::parameterCount>& info,DecodedSoundSession& result)
{
    if(!data || size<12)return false;
    const auto* bytes=static_cast<const uint8_t*>(data);
    int pos=0;
    const auto get=[&](uint32_t& value) {
        if(pos>size-4)return false;
        value=0;for(int i=0;i<4;++i)value|=uint32_t(bytes[pos++])<<(i*8);return true;
    };
    uint32_t magic=0,version=0,length=0;
    if(!get(magic) || magic!=0x32504953 || !get(version) || version<1 || version>3
        || !get(length) || !length || length>4*1024*1024 || length>static_cast<uint32_t>(size-pos))return false;
    const auto state=decodeProcessorState(bytes+pos,static_cast<int>(length),info);
    if(!state)return false;
    result.sound.state.assign(bytes+pos,bytes+pos+length);pos+=static_cast<int>(length);
    if(version>=2) {
        if(!get(length) || length>32*1024*1024 || length>static_cast<uint32_t>(size-pos))return false;
        if(!PresetBrowserState::deserialize(std::string(reinterpret_cast<const char*>(bytes+pos),length),result.sound.identity))return false;
        pos+=static_cast<int>(length);
    } else result.sound.identity.patchName=state->patchName;
    if(version==1)for(int i=0;i<ProcessorBridge::parameterCount;++i) {
        const float value=(state->values[i]-static_cast<float>(info[i].minimum))/static_cast<float>(info[i].maximum-info[i].minimum);
        result.sound.identity.baseline[i]=value;
    }
    result.size=pos;result.version=static_cast<int>(version);return true;
}
}
std::vector<uint8_t> savePresetSession(const ProcessorBridge& processor,const PresetBrowserState& browser,
    const PatchComparison* comparison)
{
    auto bytes=encodeSoundSession(processor.saveState(),browser);
    if(comparison) {
        bytes[4]=3;
        putSessionWord(bytes,static_cast<uint32_t>(comparison->activeSlot()));
        const auto& alternate=comparison->other();
        const auto other=encodeSoundSession(alternate.state,alternate.identity);
        putSessionWord(bytes,static_cast<uint32_t>(other.size()));
        bytes.insert(bytes.end(),other.begin(),other.end());
    }
    return bytes;
}
int restorePresetSession(const void* data,int size,ProcessorBridge& processor,PresetBrowserState& browser,
    PatchComparison* comparison)
{
    std::array<ParameterInfo,ProcessorBridge::parameterCount> info;
    for(int i=0;i<ProcessorBridge::parameterCount;++i)info[i]=processor.parameter(i);
    DecodedSoundSession next;
    if(!decodeSoundSession(data,size,info,next))return -1;
    uint32_t slot=0;DecodedSoundSession other;
    int consumed=next.size;
    if(next.version==3) {
        const auto* bytes=static_cast<const uint8_t*>(data);
        const auto get=[&](uint32_t& word) {
            if(consumed>size-4)return false;
            word=0;for(int i=0;i<4;++i)word|=uint32_t(bytes[consumed++])<<(i*8);return true;
        };
        uint32_t length=0;
        if(!get(slot) || slot>1 || !get(length) || length>36*1024*1024
            || length>static_cast<uint32_t>(size-consumed))return -1;
        if(!decodeSoundSession(bytes+consumed,static_cast<int>(length),info,other)
            || other.version!=2 || other.size!=static_cast<int>(length))return -1;
        consumed+=static_cast<int>(length);
    }
    if(!processor.tryRestoreState(next.sound.state.data(),static_cast<int>(next.sound.state.size())))return -1;
    browser=std::move(next.sound.identity);
    if(comparison) {
        if(next.version==3)comparison->recall(static_cast<int>(slot),std::move(other.sound));
        else comparison->reset(processor,browser);
    }
    return consumed;
}

} // namespace synth1x::host
