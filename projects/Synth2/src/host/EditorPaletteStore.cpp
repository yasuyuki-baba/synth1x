#include "EditorPaletteStore.h"
#include "tinyxml2.h"
#include <chrono>
#include <fstream>
#include <set>
#include <stdexcept>
#include <iterator>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace synth2::host
{
namespace {
template<class T> int keyIndex(const T& keys,std::string_view key) {
    for(std::size_t i=0;i<keys.size();++i)if(keys[i]==key)return static_cast<int>(i);
    return -1;
}
}
bool EditorPaletteStore::load(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error)
{
    error.clear();
    try {
        if(!std::filesystem::exists(path)){palette=editor::EditorPalette{};return true;}
        if(std::filesystem::file_size(path)>65536)throw std::runtime_error("Palette file is too large.");
        std::ifstream in(path,std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),{});
        if(!in || in.bad())throw std::runtime_error("Could not read palette preferences.");
        tinyxml2::XMLDocument doc;
        if(doc.Parse(text.data(),text.size())!=tinyxml2::XML_SUCCESS)throw std::runtime_error("Invalid palette file.");
        const auto* root=doc.RootElement();
        if(!root || std::string_view(root->Name())!="Synth2Palette" || root->IntAttribute("version")!=1)
            throw std::runtime_error("Unsupported palette file version.");
        editor::EditorPalette next;
        // Stored files contain explicit overrides only, including the default Filter overrides.
        for(auto& group:next.groups)group={};
        std::set<std::pair<int,int>> seen;
        for(auto* item=root->FirstChildElement();item;item=item->NextSiblingElement()) {
            const auto* role=item->Attribute("role");const auto* rgba=item->Attribute("rgba");
            const auto* scope=item->Attribute("group");
            const int r=role ? keyIndex(editor::paletteRoleKeys,role) : -1;
            const int g=scope ? keyIndex(editor::paletteGroupKeys,scope) : -1;
            const auto value=rgba ? editor::EditorColor::parse(rgba) : std::nullopt;
            if(std::string_view(item->Name())!="Color" || r<0 || (scope && g<0) || !value ||
               !seen.emplace(g,r).second || (g<0 && r==0 && value->alpha!=255) ||
               !next.set(static_cast<editor::PaletteRole>(r),*value,g))
                throw std::runtime_error("Invalid palette color or group.");
        }
        palette=next;return true;
    } catch(const std::filesystem::filesystem_error&) {
        error="Could not read palette preferences. Check the settings folder permissions.";return false;
    } catch(const std::exception& exception){error=exception.what();return false;}
}
bool EditorPaletteStore::save(const std::filesystem::path& path,const editor::EditorPalette& palette,std::string& error)
{
    error.clear();std::filesystem::path temporary;
    try {
        if(path.empty())throw std::runtime_error("Palette preferences folder is unavailable.");
        tinyxml2::XMLDocument doc;auto* root=doc.NewElement("Synth2Palette");root->SetAttribute("version",1);doc.InsertEndChild(root);
        const auto add=[&](std::size_t role,editor::EditorColor color,int group) {
            auto* item=doc.NewElement("Color");item->SetAttribute("role",std::string(editor::paletteRoleKeys[role]).c_str());
            item->SetAttribute("rgba",color.hex().c_str());
            if(group>=0)item->SetAttribute("group",std::string(editor::paletteGroupKeys[group]).c_str());
            root->InsertEndChild(item);
        };
        for(std::size_t r=0;r<editor::paletteRoleCount;++r)add(r,palette.global[r],-1);
        for(std::size_t g=0;g<palette.groups.size();++g)
            for(std::size_t r=0;r<editor::paletteRoleCount;++r)if(palette.groups[g][r])add(r,*palette.groups[g][r],static_cast<int>(g));
        tinyxml2::XMLPrinter printer;doc.Print(&printer);
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
        temporary=path;temporary+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<printer.CStr();out.close();
        if(!out)throw std::runtime_error("Could not write palette preferences. Check permissions and free space.");
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace palette preferences.");
#else
        std::filesystem::rename(temporary,path);
#endif
        return true;
    } catch(const std::exception& exception) {
        if(!temporary.empty()){std::error_code ignored;std::filesystem::remove(temporary,ignored);}
        error=dynamic_cast<const std::filesystem::filesystem_error*>(&exception)
            ? "Could not save the palette. Check the settings folder permissions and free space." : exception.what();
        return false;
    }
}
}
