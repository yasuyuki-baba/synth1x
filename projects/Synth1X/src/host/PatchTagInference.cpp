#include "PatchTagInference.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace synth1x::host
{
std::vector<PatchTagSuggestion> explainPatchTags(std::string_view name,const std::array<float,129>& p)
{
    const std::pair<const char*,const char*> aliases[]={{"bass","Bass"},
        {"basses","Bass"},
        {"subbass","Bass"},
        {"lead","Lead"},
        {"leads","Lead"},
        {"pad","Pad"},
        {"pads","Pad"},
        {"atmosphere","Pad"},
        {"ambient","Pad"},
        {"pluck","Pluck"},
        {"plucks","Pluck"},
        {"plucked","Pluck"},
        {"piano","Keys"},
        {"epiano","Keys"},
        {"keys","Keys"},
        {"keyboard","Keys"},
        {"clav","Keys"},
        {"clavinet","Keys"},
        {"harpsichord","Keys"},
        {"organ","Organ"},
        {"organs","Organ"},
        {"hammond","Organ"},
        {"string","Strings"},
        {"strings","Strings"},
        {"violin","Strings"},
        {"cello","Strings"},
        {"brass","Brass"},
        {"trumpet","Brass"},
        {"trombone","Brass"},
        {"horn","Brass"},
        {"woodwind","Wind"},
        {"woodwinds","Wind"},
        {"flute","Wind"},
        {"flutes","Wind"},
        {"piccolo","Wind"},
        {"clarinet","Wind"},
        {"oboe","Wind"},
        {"bassoon","Wind"},
        {"sax","Wind"},
        {"saxophone","Wind"},
        {"harmonica","Wind"},
        {"accordion","Wind"},
        {"bell","Bell"},
        {"bells","Bell"},
        {"chime","Bell"},
        {"chimes","Bell"},
        {"glockenspiel","Bell"},
        {"drum","Percussion"},
        {"drums","Percussion"},
        {"kick","Percussion"},
        {"snare","Percussion"},
        {"hihat","Percussion"},
        {"hat","Percussion"},
        {"clap","Percussion"},
        {"tom","Percussion"},
        {"perc","Percussion"},
        {"percussion","Percussion"},
        {"cymbal","Percussion"},
        {"fx","FX"},
        {"sfx","FX"},
        {"effect","FX"},
        {"effects","FX"},
        {"laser","FX"},
        {"explosion","FX"},
        {"sequence","Sequence"},
        {"sequences","Sequence"},
        {"sequenced","Sequence"},
        {"sequencer","Sequence"},
        {"arp","Arp"},
        {"arpeggio","Arp"},
        {"arpeggiator","Arp"},
        {"arpeggiated","Arp"}};
    const std::pair<const char*,const char*> prefixes[]={{"bs","Bass"},
        {"bas","Bass"},
        {"ba","Bass"},
        {"ld","Lead"},
        {"led","Lead"},
        {"pd","Pad"},
        {"plk","Pluck"},
        {"kys","Keys"},
        {"ky","Keys"},
        {"key","Keys"},
        {"pno","Keys"},
        {"org","Organ"},
        {"str","Strings"},
        {"brs","Brass"},
        {"wnd","Wind"},
        {"seq","Sequence"},
        {"bl","Bell"},
        {"prc","Percussion"},
        {"drm","Percussion"}};
    const auto upper=[](char c){return c>='A' && c<='Z';};
    const auto lower=[](char c){return c>='a' && c<='z';};
    const auto letter=[&](char c){return upper(c) || lower(c);};
    std::vector<std::string> words;
    for(std::size_t start=0;start<name.size();) {
        if(!letter(name[start])){++start;continue;}
        auto end=start;std::string token;
        while(end<name.size() && letter(name[end])) {
            const auto c=name[end++];token+=upper(c) ? static_cast<char>(c-'A'+'a') : c;
        }
        // Whole known words win over acronym splitting: PADs and BASSoon must
        // not become "pa ds" and "bas soon" respectively.
        if(std::any_of(std::begin(aliases),std::end(aliases),[&](const auto& alias){return token==alias.first;}))
            words.push_back(std::move(token));
        else {
            auto part=start;
            for(auto i=start+1;i<end;++i) {
                if(upper(name[i]) && (lower(name[i-1])
                    || (upper(name[i-1]) && i+1<end && lower(name[i+1]) && i-part>1))) {
                    words.push_back(token.substr(part-start,i-part));part=i;
                }
            }
            words.push_back(token.substr(part-start));
        }
        start=end;
    }
    std::vector<PatchTagSuggestion> tags;
    const auto add=[&](std::string tag,std::string rule,std::string detail) {
        auto found=std::find_if(tags.begin(),tags.end(),[&](const auto& item){return item.tag==tag;});
        if(found==tags.end()){tags.push_back({std::move(tag),{}});found=tags.end()-1;}
        PatchTagReason reason{std::move(rule),std::move(detail)};
        if(std::find(found->reasons.begin(),found->reasons.end(),reason)==found->reasons.end())
            found->reasons.push_back(std::move(reason));
    };
    const auto seconds=[](double value) {
        std::ostringstream text;text.imbue(std::locale::classic());
        text<<std::fixed<<std::setprecision(4)<<value<<" s";return text.str();
    };
    for(const auto& token:words)for(const auto& [match,tag]:aliases)
        if(token==match)add(tag,"name.word","Name contains the word '"+token+"'.");
    if(!words.empty())for(const auto& [match,tag]:prefixes)
        if(words.front()==match)add(tag,"name.prefix","Name starts with category '"+words.front()+"'.");
    const auto attack=[](float dial) {return .0005*std::pow(56000.,std::clamp(dial,0.f,127.f)/127.);};
    const auto decay=[](float dial) {return .01*std::pow(6000.,std::clamp(dial,0.f,127.f)/127.);};
    const double a=attack(p[25]),d=decay(p[26]),release=decay(p[28]);
    const double raw=std::clamp(p[27],0.f,127.f);
    const double sustain=raw==0 ? 0 : std::floor(std::pow(2048.,std::pow(raw/127.,.3)-1)*2048)/2048;
    // Named categories describe intended use. Envelope features can coexist;
    // only Arp asserts a concrete enabled feature and therefore follows settings.
    std::erase_if(tags,[&](const auto& suggestion) {
        return suggestion.tag=="Arp" && p[59]==0;
    });
    const bool filterPluck=(p[14]==0 || p[14]==1 || p[14]==4) && attack(p[15])<=.03
        && decay(p[16])<=2 && p[17]<=.3*127 && p[21]>=80 && p[19]<=96;
    if(filterPluck)add("Filter pluck","settings.filter-envelope","Fast low-pass envelope, low sustain, positive depth and restrained base cutoff.");
    if(a>=.15)add("Slow attack","settings.amp-attack","Amp attack is "+seconds(a)+" (at least 0.15 s).");
    if(release>=1)add("Long release","settings.amp-release","Amp release is "+seconds(release)+" (at least 1 s).");
    if(a<=.03 && sustain<=.12 && d<=2)add("Short decay","settings.amp-decay","Fast amp attack, low sustain and decay of "+seconds(d)+" (at most 2 s).");
    if(p[1]==4 && p[5]>=.15*127)add("Noise","settings.osc-noise","OSC2 uses Noise and OSC Mix is at least 15% toward OSC2.");
    if(p[7]==0 && p[45]>=8)add("FM","settings.osc-fm","OSC1 FM Amount is at least 8 and Ring Mod is off.");
    if(p[6]!=0)add("Sync","settings.osc-sync","OSC Sync is enabled.");
    if(p[7]!=0)add("Ring mod","settings.osc-ring","Oscillator Ring Mod is enabled.");
    if(p[59]!=0)add("Arp","settings.arpeggiator","The arpeggiator is enabled.");
    if(p[73]!=0 && p[93]>=2)add("Unison","settings.unison","Unison is enabled with at least two voices.");
    if(p[38]==0)add("Poly","settings.voice-mode","Voice mode is Poly.");
    if(p[38]==1)add("Mono","settings.voice-mode","Voice mode is Mono.");
    if(p[38]==2)add("Legato","settings.voice-mode","Voice mode is Legato.");
    if(p[77]!=0 && p[78]>=0 && p[78]<=3)add("Distortion","settings.effect","The effect is enabled with an overdrive or decimator type.");
    return tags;
}
}
