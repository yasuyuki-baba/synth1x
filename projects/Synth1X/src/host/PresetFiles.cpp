#include "PresetFiles.h"
#include "PatchComparison.h"
#include "ProcessorState.h"
#include "PatchLibrary.h"
#include "LibraryFiles.h"
#include "patch/PatchFormat.h"
#include "patch/PatchLoader.h"
#include <tinyxml2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif

namespace synth1x::host
{
namespace fs = std::filesystem;
namespace
{
constexpr char patchWritePrefix[] = ".patch-write-";

std::string utf8(const fs::path &p)
{
    auto s = p.generic_u8string();
    return {s.begin(), s.end()};
}
std::string identity(const std::string &path, const std::string &entry = {})
{
    return std::to_string(path.size()) + ":" + path + entry;
}
bool inside(const fs::path &parent, const fs::path &child)
{
    auto p = fs::weakly_canonical(parent), c = fs::weakly_canonical(child);
    auto pi = p.begin(), ci = c.begin();
    for (; pi != p.end(); ++pi, ++ci)
        if (ci == c.end() || *pi != *ci)
            return false;
    return true;
}
std::string validate(const std::string &text)
{
    ProcessorBridge validator;
    std::string error;
    if (!validator.loadPatch(text, error))
        return error;
    return {};
}
// A sibling temporary is renamed only after a complete write. Never truncate the
// existing destination. Exclusive temporary creation avoids sharing instances.
bool writeFile(const fs::path &destination, const std::string &bytes, bool replace, std::string &error)
{
    fs::path staging;
    try
    {
        fs::create_directories(destination.parent_path());
        static std::atomic<unsigned> serial{};
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            staging = destination.parent_path() /
                      (patchWritePrefix +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                       std::to_string(serial++));
            if (fs::create_directory(staging))
                break;
            staging.clear();
        }
        if (staging.empty())
            throw std::runtime_error("Cannot create temporary directory.");
        auto temporary = staging / "data";
        {
            std::ofstream out(temporary, std::ios::binary);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if (!out)
                throw std::runtime_error("Cannot write the complete preset file.");
            out.close();
            if (!out)
                throw std::runtime_error("Cannot close the preset file.");
        }
#ifdef _WIN32
        const auto flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), flags))
            throw std::runtime_error("Cannot publish the destination file.");
#elif defined(__APPLE__)
        if (renamex_np(temporary.c_str(), destination.c_str(), replace ? 0 : RENAME_EXCL) != 0)
            throw std::runtime_error("Cannot publish the destination file.");
#else
        if (!replace)
        {
            // Publish without overwriting a concurrently created destination.
            fs::create_hard_link(temporary, destination);
        }
        else
            fs::rename(temporary, destination);
#endif
        std::error_code cleanupError;
        fs::remove_all(staging, cleanupError);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        if (!staging.empty())
        {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
        }
        return false;
    }
}
std::string readFile(const fs::path &path,std::uintmax_t maximumBytes=PatchLibrary::maximumBankBytes)
{
    auto size = fs::file_size(path);
    if (size > maximumBytes)
        throw std::runtime_error("File exceeds the size limit.");
    std::ifstream in(path, std::ios::binary);
    std::string s(static_cast<std::size_t>(size), '\0');
    if (!in.read(s.data(), s.size()) || in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Cannot read the complete file.");
    return s;
}
fs::path environmentPath(const char *name)
{
#ifdef _WIN32
    std::wstring key;
    while (*name)
        key += static_cast<wchar_t>(*name++);
    auto n = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (!n)
        return {};
    std::wstring value(n, L'\0');
    GetEnvironmentVariableW(key.c_str(), value.data(), n);
    value.resize(n - 1);
    return fs::path(value);
#else
    auto value = std::getenv(name);
    return value ? fs::path(value) : fs::path{};
#endif
}
} // namespace
PresetFiles::PresetFiles(fs::path directory) : root(fs::weakly_canonical(fs::absolute(std::move(directory))))
{
}
bool PresetFiles::isManagedBank(const fs::path &directory, const fs::path &bank)
{
    return bank.parent_path().lexically_normal() == (directory / "banks").lexically_normal() &&
           utf8(bank.filename()).starts_with("bank-") && fs::is_directory(fs::symlink_status(bank)) &&
           fs::is_regular_file(fs::symlink_status(bank / "name"));
}
bool PresetFiles::isManagedPatch(const fs::path &directory, const fs::path &file)
{
    if (libraryFileKind(file) != LibraryFileKind::patch || !fs::is_regular_file(fs::symlink_status(file)))
        return false;
    const auto bank = file.parent_path();
    if (!isManagedBank(directory, bank))
        return false;
    const auto key = utf8(file.stem());
    if (key.empty() || key.size() > std::to_string(PatchLibrary::maximumPatches - 1).size() || key.find_first_not_of("0123456789") != std::string::npos ||
        std::stoi(key) >= static_cast<int>(PatchLibrary::maximumPatches))
        return false;
    return fs::is_regular_file(fs::symlink_status(bank / (key + ".name")));
}
bool PresetFiles::scan(std::string &error, bool validateSources)
{
    try
    {
        fs::create_directories(root);
        std::vector<UserPreset> next;
        std::size_t total = 0;
        auto append = [&](UserPreset item)
        {
            if (next.size() >= 100000 || item.text.size() > 512u * 1024u * 1024u - total)
                throw std::runtime_error("The library exceeds its size limit.");
            total += item.text.size();
            next.push_back(std::move(item));
        };
        for (const auto &item :
             fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
        {
            if (item.is_symlink() || !item.is_regular_file() || libraryFileKind(item.path()) == LibraryFileKind::unsupported)
                continue;
            const auto relative = utf8(item.path().lexically_relative(root));
            const auto first = utf8(*item.path().lexically_relative(root).begin());
            if (isManagedPatch(root, item.path()) || first.starts_with(patchWritePrefix) ||
                first.starts_with(".synth2-write-"))
                continue;
            PatchLibrary bank;
            std::string failure;
            if (!bank.openFile(utf8(item.path()), failure))
            {
                append(
                    {identity(relative), utf8(item.path().stem()), relative, {}, {}, failure, item.path()});
                continue;
            }
            int ordinal = 0;
            for (const auto &patch : bank.patches())
            {
                auto docName = patch.name;
                const auto& entry = patch.entry;
                try
                {
                    const auto name = synth1x::isContinuousPatch(patch.text)
                                          ? synth1x::parseContinuousPatch(patch.text).name
                                          : parsePatchDocument(patch.text).name;
                    if (!name.empty())
                        docName = name;
                }
                catch (const std::exception &)
                {
                }
                auto problem = validateSources ? validate(patch.text) : std::string{};
                if (docName.size() > 16384)
                {
                    docName = utf8(item.path().filename());
                    problem = "The patch name exceeds the library name limit.";
                }
                append({identity(relative, entry), docName, relative + (entry.empty() ? "" : " / " + entry), entry,
                        patch.text, problem, item.path()});
                next.back().program = ordinal++;
            }
        }
        std::sort(next.begin(), next.end(),
                  [](const auto &a, const auto &b)
                  {
                      if (naturalLibraryPathLess(a.location, b.location))
                          return true;
                      if (naturalLibraryPathLess(b.location, a.location))
                          return false;
                      return a.id < b.id;
                  });
        items.swap(next);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
PresetImportResult PresetFiles::import(const fs::path &inputSource, const ResolveConflict &resolve)
{
    PresetImportResult result;
    std::vector<fs::path> files;
    std::vector<std::string> registered;
    try
    {
        const auto source = fs::weakly_canonical(inputSource);
        bool directory = fs::is_directory(source);
        if (directory && source != root && inside(source, root))
            throw std::runtime_error("The source folder contains the preset directory.");
        if (directory)
        {
            for (const auto &e :
                 fs::recursive_directory_iterator(source, fs::directory_options::skip_permission_denied))
                if (!e.is_symlink() && e.is_regular_file() && libraryFileKind(e.path()) != LibraryFileKind::unsupported)
                    files.push_back(e.path());
        }
        else
        {
            if (libraryFileKind(source) == LibraryFileKind::unsupported)
                throw std::runtime_error("Choose a .sy1 file, ZIP, RAR, 7z or folder.");
            files.push_back(source);
        }
        std::sort(files.begin(), files.end());
        bool all = false;
        FileConflict policy = FileConflict::keepBoth;
        for (const auto &file : files)
        {
            try
            {
                fs::path destination;
                if (inside(root, file))
                    destination = fs::weakly_canonical(file);
                else
                {
                    auto relative =
                        directory ? source.filename() / file.lexically_relative(source) : file.filename();
                    destination = root / relative;
                    if (!inside(root, destination))
                        throw std::runtime_error("Destination escapes the preset directory.");
                    bool replace = false;
                    if (fs::exists(destination))
                    {
                        auto choice = all ? ConflictChoice{policy, true} : resolve(destination);
                        policy = choice.action;
                        all = choice.applyToAll;
                        if (policy == FileConflict::skip)
                        {
                            ++result.skipped;
                            continue;
                        }
                        if (policy == FileConflict::keepBoth)
                        {
                            auto original = destination;
                            for (unsigned n = 2; fs::exists(destination); ++n)
                                destination = original.parent_path() /
                                              (utf8(original.stem()) + " (" + std::to_string(n) + ")" +
                                               utf8(original.extension()));
                        }
                        else
                            replace = true;
                    }
                    std::string failure;
                    if (!writeFile(destination, readFile(file), replace, failure))
                        throw std::runtime_error(failure);
                }
                registered.push_back(utf8(destination.lexically_relative(root)));
                ++result.imported;
            }
            catch (const std::exception &e)
            {
                result.errors.push_back(utf8(file) + ": " + e.what());
            }
        }
    }
    catch (const std::exception &e)
    {
        result.errors.push_back(e.what());
    }
    std::string failure;
    if (!scan(failure))
        result.errors.push_back(failure);
    for (const auto &item : items)
        if (std::find(registered.begin(), registered.end(), utf8(item.file.lexically_relative(root))) !=
            registered.end())
        {
            result.ids.push_back(item.id);
            if (!item.error.empty())
                result.errors.push_back(item.location + ": " + item.error);
        }
    return result;
}
bool PresetFiles::save(const fs::path &relative, const std::string &text, bool replace, std::string &id,
                       std::string &error)
{
    try
    {
        if (relative.empty() || relative.is_absolute() || libraryFileKind(relative) != LibraryFileKind::patch ||
            !inside(root, root / relative))
            throw std::runtime_error("Save a .sy1 file inside the preset directory.");
        if (auto failure = validate(text); !failure.empty())
            throw std::runtime_error(failure);
        if (!writeFile(root / relative, text, replace, error))
            return false;
        id = identity(utf8((root / relative).lexically_normal().lexically_relative(root)));
        return scan(error);
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
fs::path PresetFiles::settingsDirectory()
{
#ifdef _WIN32
    auto home = environmentPath("APPDATA");
#elif defined(__APPLE__)
    auto home = environmentPath("HOME");
    if (!home.empty())
        home /= "Library/Application Support";
#else
    auto home = environmentPath("XDG_DATA_HOME");
    if (home.empty())
    {
        home = environmentPath("HOME");
        if (!home.empty())
            home /= ".local/share";
    }
#endif
    if (home.empty())
        throw std::runtime_error("The user data directory is unavailable.");
    return home / "Synth1X";
}
fs::path PresetFiles::defaultDirectory() { return settingsDirectory() / "Library"; }
fs::path PresetFiles::configuredDirectory()
{
    try
    {
        auto file = settingsDirectory() / "preset-directory";
        if (!fs::exists(file))
            return defaultDirectory();
        auto value = readFile(file);
        if (value.empty())
            return defaultDirectory();
        auto p = fs::path(std::u8string(value.begin(), value.end()));
        return p.is_absolute() ? p : defaultDirectory();
    }
    catch (const std::exception &)
    {
        return defaultDirectory();
    }
}

bool PresetFiles::configureDirectory(const fs::path &path, std::string &error)
{
    try
    {
        fs::create_directories(path);
        return writeFile(settingsDirectory() / "preset-directory",
                         utf8(fs::weakly_canonical(fs::absolute(path))), true, error);
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
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
bool PresetFiles::saveStandaloneState(const std::vector<uint8_t> &bytes, std::string &error)
{
    return writeFile(settingsDirectory() / "standalone.state", std::string(bytes.begin(), bytes.end()), true,
                     error);
}
std::vector<uint8_t> PresetFiles::loadStandaloneState()
{
    try
    {
        auto text = readFile(settingsDirectory() / "standalone.state",80*1024*1024);
        if (text.size() > 80 * 1024 * 1024)
            return {};
        return {text.begin(), text.end()};
    }
    catch (const std::exception &)
    {
        return {};
    }
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
