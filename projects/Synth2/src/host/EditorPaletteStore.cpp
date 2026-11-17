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
constexpr std::string_view base64Alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string encodeImage(const std::vector<std::uint8_t>& bytes)
{
    std::string result;result.reserve((bytes.size()+2)/3*4);
    for(std::size_t i=0;i<bytes.size();i+=3) {
        const auto b=static_cast<std::uint32_t>(bytes[i])<<16 |
            (i+1<bytes.size() ? static_cast<std::uint32_t>(bytes[i+1])<<8 : 0) |
            (i+2<bytes.size() ? bytes[i+2] : 0);
        result+=base64Alphabet[(b>>18)&63];result+=base64Alphabet[(b>>12)&63];
        result+=i+1<bytes.size() ? base64Alphabet[(b>>6)&63] : '=';
        result+=i+2<bytes.size() ? base64Alphabet[b&63] : '=';
    }
    return result;
}
std::vector<std::uint8_t> decodeImage(std::string_view text)
{
    if(text.empty() || text.size()%4 || text.size()>(editor::EditorBackgroundImage::maximumBytes+2)/3*4)
        throw std::runtime_error("Invalid or oversized background image data.");
    std::vector<std::uint8_t> bytes;bytes.reserve(text.size()/4*3);
    for(std::size_t i=0;i<text.size();i+=4) {
        std::uint32_t bits=0;int padding=0;
        for(int j=0;j<4;++j) {
            const char c=text[i+j];
            if(c=='=' && j>=2 && i+4==text.size()){++padding;bits<<=6;}
            else {
                const auto index=base64Alphabet.find(c);
                if(index==std::string_view::npos || padding)throw std::runtime_error("Invalid background image encoding.");
                bits=(bits<<6)|static_cast<std::uint32_t>(index);
            }
        }
        if((padding==1 && (bits&255)) || (padding==2 && (bits&65535)))
            throw std::runtime_error("Invalid background image padding.");
        bytes.push_back(static_cast<std::uint8_t>(bits>>16));
        if(padding<2)bytes.push_back(static_cast<std::uint8_t>(bits>>8));
        if(!padding)bytes.push_back(static_cast<std::uint8_t>(bits));
    }
    return bytes;
}
template<class T> int keyIndex(const T& keys,std::string_view key) {
    for(std::size_t i=0;i<keys.size();++i)if(keys[i]==key)return static_cast<int>(i);
    return -1;
}
bool readPalette(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error,bool missingUsesDefaults)
{
    error.clear();
    try {
        if(!std::filesystem::exists(path)) {
            if(!missingUsesDefaults)throw std::runtime_error("The selected palette file no longer exists.");
            palette=editor::EditorPalette{};return true;
        }
        if(std::filesystem::file_size(path)>12*1024*1024)throw std::runtime_error("Palette file is too large.");
        std::ifstream in(path,std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),{});
        if(!in || in.bad())throw std::runtime_error("Could not read palette preferences.");
        tinyxml2::XMLDocument doc;
        if(doc.Parse(text.data(),text.size())!=tinyxml2::XML_SUCCESS)throw std::runtime_error("Invalid palette file.");
        const auto* root=doc.RootElement();
        if(!root || std::string_view(root->Name())!="Synth2Palette" || (root->IntAttribute("version")!=1 && root->IntAttribute("version")!=2))
            throw std::runtime_error("Unsupported palette file version.");
        const int version=root->IntAttribute("version");
        if(version==1 && text.size()>65536)throw std::runtime_error("Palette file is too large.");
        editor::EditorPalette next;
        // Stored files contain explicit overrides only, including the default Filter overrides.
        for(auto& group:next.groups)group={};
        std::set<std::pair<int,int>> seen;
        for(auto* item=root->FirstChildElement();item;item=item->NextSiblingElement()) {
            if(std::string_view(item->Name())=="BackgroundImage") {
                if(version!=2 || next.backgroundImage || !item->GetText() ||
                   !item->Attribute("encoding") || std::string_view(item->Attribute("encoding"))!="base64")
                    throw std::runtime_error("Invalid background image entry.");
                auto image=std::make_shared<editor::EditorBackgroundImage>();
                image->name=item->Attribute("name") ? item->Attribute("name") : "Background image";
                if(image->name.size()>1024)throw std::runtime_error("Background image name is too long.");
                image->bytes=decodeImage(item->GetText());
                if(!image->supported())throw std::runtime_error("Choose a PNG or JPEG image up to 8 MiB.");
                next.backgroundImage=std::move(image);continue;
            }
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
        if(version==2 && !next.backgroundImage)throw std::runtime_error("Missing background image data.");
        palette=next;return true;
    } catch(const std::filesystem::filesystem_error&) {
        error="Could not read palette preferences. Check the settings folder permissions.";return false;
    } catch(const std::exception& exception){error=exception.what();return false;}
}
}
bool EditorPaletteStore::load(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error)
{
    return readPalette(path,palette,error,true);
}
bool EditorPaletteStore::importFile(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error)
{
    return readPalette(path,palette,error,false);
}
bool EditorPaletteStore::readImage(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error)
{
    error.clear();
    try {
        if(std::filesystem::file_size(path)>editor::EditorBackgroundImage::maximumBytes)
            throw std::runtime_error("Choose a PNG or JPEG image up to 8 MiB.");
        auto image=std::make_shared<editor::EditorBackgroundImage>();
        const auto name=path.filename().u8string();image->name.assign(name.begin(),name.end());
        std::ifstream in(path,std::ios::binary);
        image->bytes.assign(std::istreambuf_iterator<char>(in),{});
        if(!in || in.bad())throw std::runtime_error("Could not read the background image.");
        if(!image->supported())throw std::runtime_error("Choose a PNG or JPEG image up to 8 MiB.");
        palette.backgroundImage=std::move(image);return true;
    } catch(const std::exception&){error="Could not read the background image. Choose a PNG or JPEG up to 8 MiB.";return false;}
}
bool EditorPaletteStore::save(const std::filesystem::path& path,const editor::EditorPalette& palette,std::string& error)
{
    error.clear();std::filesystem::path temporary;
    try {
        if(path.empty())throw std::runtime_error("Palette preferences folder is unavailable.");
        tinyxml2::XMLDocument doc;auto* root=doc.NewElement("Synth2Palette");root->SetAttribute("version",palette.backgroundImage ? 2 : 1);doc.InsertEndChild(root);
        const auto add=[&](std::size_t role,editor::EditorColor color,int group) {
            auto* item=doc.NewElement("Color");item->SetAttribute("role",std::string(editor::paletteRoleKeys[role]).c_str());
            item->SetAttribute("rgba",color.hex().c_str());
            if(group>=0)item->SetAttribute("group",std::string(editor::paletteGroupKeys[group]).c_str());
            root->InsertEndChild(item);
        };
        for(std::size_t r=0;r<editor::paletteRoleCount;++r)add(r,palette.global[r],-1);
        for(std::size_t g=0;g<palette.groups.size();++g)
            for(std::size_t r=0;r<editor::paletteRoleCount;++r)if(palette.groups[g][r])add(r,*palette.groups[g][r],static_cast<int>(g));
        if(palette.backgroundImage) {
            if(!palette.backgroundImage->supported() || palette.backgroundImage->name.size()>1024)
                throw std::runtime_error("Invalid background image.");
            auto* image=doc.NewElement("BackgroundImage");image->SetAttribute("encoding","base64");
            image->SetAttribute("name",palette.backgroundImage->name.c_str());
            image->SetText(encodeImage(palette.backgroundImage->bytes).c_str());root->InsertEndChild(image);
        }
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
