// Read-only tag diagnostics using the product's source readers and loading bridge.
#include "host/PatchLibrary.h"
#include "host/PatchTagInference.h"
#include "host/PresetFiles.h"
#include "host/ProcessorBridge.h"
#include "patch/PatchFormat.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
std::string utf8(const std::filesystem::path& path)
{
    const auto bytes=path.generic_u8string();return {bytes.begin(),bytes.end()};
}
void jsonString(std::ostream& out,std::string_view value)
{
    constexpr char hex[]="0123456789abcdef";
    out<<'"';
    for(std::size_t i=0;i<value.size();) {
        const auto c=static_cast<unsigned char>(value[i]);
        if(c<128) {
            if(c=='"' || c=='\\')out<<'\\'<<static_cast<char>(c);
            else if(c<32)out<<"\\u00"<<hex[c>>4]<<hex[c&15];
            else out<<static_cast<char>(c);
            ++i;continue;
        }
        // Legacy titles may contain non-UTF-8 bytes. Replace invalid display text,
        // while source_hex below retains exact source bytes for comparison hashes.
        const int length=c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
        unsigned code=length ? c&((1u<<(7-length))-1) : 0;
        bool valid=length && i+length<=value.size();
        for(int n=1;valid && n<length;++n) {
            const auto next=static_cast<unsigned char>(value[i+n]);
            valid=(next&0xc0)==0x80;code=(code<<6)|(next&0x3f);
        }
        const unsigned minimum=length==2 ? 0x80 : length==3 ? 0x800 : 0x10000;
        valid=valid && code>=minimum && code<=0x10ffff && !(code>=0xd800 && code<=0xdfff);
        if(valid){out.write(value.data()+i,length);i+=length;}
        else {out<<"\\ufffd";++i;}
    }
    out<<'"';
}
std::vector<synth1x::host::UserPreset> readSources(const std::filesystem::path& source)
{
    std::string error;
    if(std::filesystem::is_directory(source)) {
        synth1x::host::PresetFiles files(source);
        if(!files.scan(error,false))throw std::runtime_error(error);
        return files.entries();
    }
    synth1x::host::PatchLibrary bank;
    if(!bank.openFile(utf8(source),error))throw std::runtime_error(error);
    std::vector<synth1x::host::UserPreset> result;
    const auto relative=utf8(source.filename());
    for(const auto& patch:bank.patches()) {
        const auto id=std::to_string(relative.size())+":"+relative+patch.entry;
        result.push_back({id,patch.name,relative,patch.entry,patch.text,{},source});
    }
    return result;
}
int run(const std::filesystem::path& source)
{
    const auto patches=readSources(source);
    synth1x::host::ProcessorBridge bridge;
    std::cout<<"{\"inference_revision\":"<<synth1x::host::patchTagCacheVersion<<"}\n";
    bool failed=false;
    for(const auto& patch:patches) {
        auto error=patch.error;std::string name=patch.name;
        std::vector<synth1x::host::PatchTagSuggestion> suggestions;
        if(error.empty() && bridge.loadPatch(patch.text,error)) {
            name=bridge.currentPatchName();
            suggestions=synth1x::host::explainPatchTags(name,synth1x::parseContinuousPatch(bridge.savePatch()).values);
        }
        failed=failed || !error.empty();
        std::cout<<"{\"id\":";jsonString(std::cout,patch.id);
        std::cout<<",\"name\":";jsonString(std::cout,name);
        std::cout<<",\"source_hex\":\"";
        constexpr char hex[]="0123456789abcdef";
        for(unsigned char byte:patch.text)std::cout<<hex[byte>>4]<<hex[byte&15];
        std::cout<<"\",\"error\":";jsonString(std::cout,error);
        std::cout<<",\"tags\":[";
        bool first=true;
        for(const auto& suggestion:suggestions) {
            if(!first)std::cout<<',';first=false;
            std::cout<<"{\"tag\":";jsonString(std::cout,suggestion.tag);
            std::cout<<",\"reasons\":[";
            bool firstReason=true;
            for(const auto& reason:suggestion.reasons) {
                if(!firstReason)std::cout<<',';firstReason=false;
                std::cout<<"{\"rule\":";jsonString(std::cout,reason.rule);
                std::cout<<",\"detail\":";jsonString(std::cout,reason.detail);std::cout<<'}';
            }
            std::cout<<"]}";
        }
        std::cout<<"]}\n";
    }
    if(!std::cout)throw std::runtime_error("Could not write the complete tag report.");
    return failed ? 2 : 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
#else
int main(int argc,char** argv)
#endif
{
    try {
        if(argc!=2)throw std::runtime_error("Usage: Synth1XTagProbe PATCH_OR_ARCHIVE_OR_DIRECTORY");
        return run(std::filesystem::path(argv[1]));
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
