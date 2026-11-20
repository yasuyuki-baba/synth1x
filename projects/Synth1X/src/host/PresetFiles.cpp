#include "PresetFiles.h"
#include "ProcessorBridge.h"
#include "PatchLibrary.h"
#include "LibraryFiles.h"
#include "patch/PatchFormat.h"
#include "patch/PatchLoader.h"
#include <archive_blake2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
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

// Bump this revision when archive decoding or catalog entry construction changes.
constexpr std::string_view archiveCacheHeader = "archive cache 1\n";
constexpr std::size_t archiveCacheLimit = 128u * 1024u * 1024u;
using ArchiveDigest = std::array<char, BLAKE2S_OUTBYTES>;

ArchiveDigest archiveDigest(std::string_view bytes)
{
    ArchiveDigest digest;
    if (blake2s(digest.data(), digest.size(), bytes.data(), bytes.size(), nullptr, 0) != 0)
        throw std::runtime_error("Cannot fingerprint the archive.");
    return digest;
}
void appendArchiveField(std::string& record, std::string_view field)
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        record.push_back(static_cast<char>((field.size() >> shift) & 0xffu));
    record.append(field);
}
std::string_view takeArchiveField(std::string_view& record)
{
    if (record.size() < 4)
        throw std::runtime_error("Truncated archive cache field.");
    std::uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i)
        size |= static_cast<std::uint32_t>(static_cast<unsigned char>(record[i])) << (8 * i);
    record.remove_prefix(4);
    if (size > record.size())
        throw std::runtime_error("Invalid archive cache field size.");
    const auto field = record.substr(0, size);
    record.remove_prefix(size);
    return field;
}

// Records store the archive path, its compressed-byte digest and a checked
// snapshot of the entries. File metadata alone cannot identify archive contents.
class ArchiveScanCache
{
public:
    void load(const fs::path& root)
    {
        path = root / ".archives.cache";
        try
        {
            if (!fs::is_regular_file(fs::symlink_status(path)))
                return;
            previous = readFile(path, archiveCacheLimit);
            if (!previous.starts_with(archiveCacheHeader))
                return;
            std::string_view remaining(previous);
            remaining.remove_prefix(archiveCacheHeader.size());
            while (!remaining.empty())
            {
                const auto relative = takeArchiveField(remaining);
                const auto snapshot = takeArchiveField(remaining);
                if (relative.empty() || records.size() == 100000 ||
                    !records.emplace(relative, snapshot).second)
                    throw std::runtime_error("Invalid archive cache index.");
            }
        }
        catch (const std::exception&)
        {
            records.clear();
        }
    }

    std::optional<std::vector<UserPreset>> restore(std::string_view relative,
        const fs::path& file, const ArchiveDigest& source)
    {
        const auto found = records.find(relative);
        if (found == records.end())
            return std::nullopt;
        const auto snapshot = found->second;
        const std::string_view sourceKey(source.data(), source.size());
        if (snapshot.size() <= 2 * source.size() || !snapshot.starts_with(sourceKey))
            return std::nullopt;
        auto payload = snapshot.substr(2 * source.size());
        const auto checksum = archiveDigest(payload);
        if (snapshot.substr(source.size(), checksum.size()) != std::string_view(checksum.data(), checksum.size()))
            return std::nullopt;
        try
        {
            std::vector<UserPreset> patches;
            const std::string relativePath(relative);
            std::size_t total = 0;
            while (!payload.empty())
            {
                const auto entry = takeArchiveField(payload);
                const auto name = takeArchiveField(payload);
                const auto text = takeArchiveField(payload);
                const auto error = takeArchiveField(payload);
                if (patches.size() == PatchLibrary::maximumPatches || entry.empty() ||
                    name.size() > 16384 || text.empty() || text.size() > PatchLibrary::maximumPatchBytes ||
                    text.size() > PatchLibrary::maximumBankBytes - total)
                    return std::nullopt;
                total += text.size();
                const std::string archiveEntry(entry);
                patches.push_back({identity(relativePath, archiveEntry), std::string(name),
                    relativePath + " / " + archiveEntry, archiveEntry, std::string(text),
                    std::string(error), file, static_cast<int>(patches.size())});
            }
            keep(relative, snapshot);
            return patches;
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }

    void store(std::string_view relative, const ArchiveDigest& source, std::span<const UserPreset> patches)
    {
        std::size_t size = relative.size() + 8 + 2 * source.size();
        for (const auto& patch : patches)
        {
            size += patch.archiveEntry.size() + patch.name.size() + patch.text.size() + patch.error.size() + 16;
            if (size > archiveCacheLimit - updated.size())
                return;
        }
        std::string snapshot(source.data(), source.size());
        snapshot.resize(2 * source.size());
        snapshot.reserve(size - relative.size() - 8);
        for (const auto& patch : patches)
        {
            appendArchiveField(snapshot, patch.archiveEntry);
            appendArchiveField(snapshot, patch.name);
            appendArchiveField(snapshot, patch.text);
            appendArchiveField(snapshot, patch.error);
        }
        const auto checksum = archiveDigest(std::string_view(snapshot).substr(2 * source.size()));
        std::copy(checksum.begin(), checksum.end(), snapshot.begin() + source.size());
        keep(relative, snapshot);
    }

    void publish() const
    {
        if (!path.empty() && updated != previous && (updated != archiveCacheHeader || !previous.empty()))
        {
            std::string ignored;
            writeFile(path, updated, true, ignored);
        }
    }

private:
    void keep(std::string_view relative, std::string_view snapshot)
    {
        if (relative.size() + snapshot.size() + 8 > archiveCacheLimit - updated.size())
            return;
        appendArchiveField(updated, relative);
        appendArchiveField(updated, snapshot);
    }

    fs::path path;
    std::string previous, updated{archiveCacheHeader};
    std::unordered_map<std::string_view, std::string_view> records;
};

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
bool PresetFiles::scan(std::string &error, bool validateSources, std::size_t* reusedArchives)
{
    try
    {
        fs::create_directories(root);
        const bool useArchiveCache = reusedArchives && !validateSources;
        ArchiveScanCache archiveCache;
        std::size_t archiveHits = 0;
        if (useArchiveCache)
            archiveCache.load(root);
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
            const auto kind = libraryFileKind(item.path());
            if (item.is_symlink() || !item.is_regular_file() || kind == LibraryFileKind::unsupported)
                continue;
            const auto relative = utf8(item.path().lexically_relative(root));
            const auto first = utf8(*item.path().lexically_relative(root).begin());
            if (isManagedPatch(root, item.path()) || first.starts_with(patchWritePrefix) ||
                first.starts_with(".synth2-write-"))
                continue;
            PatchLibrary bank;
            std::string failure;
            auto data = PatchLibrary::readSource(item.path(), failure);
            std::optional<ArchiveDigest> digest;
            if (useArchiveCache && isLibraryArchive(kind) && !data.empty())
            {
                digest = archiveDigest(data);
                if (auto cached = archiveCache.restore(relative, item.path(), *digest))
                {
                    for (auto& patch : *cached)
                        append(std::move(patch));
                    ++archiveHits;
                    continue;
                }
            }
            if (data.empty() || !bank.openSource(item.path(), std::move(data), failure))
            {
                append(
                    {identity(relative), utf8(item.path().stem()), relative, {}, {}, failure, item.path()});
                continue;
            }
            const auto start = next.size();
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
            if (digest)
                archiveCache.store(relative, *digest, std::span<const UserPreset>(next).subspan(start));
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
        archiveCache.publish();
        items.swap(next);
        if (reusedArchives)
            *reusedArchives = archiveHits;
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
    const auto registered = copySources(inputSource, resolve, result);
    std::string failure;
    if (!scan(failure))
        result.errors.push_back(failure);
    for (const auto &item : items)
        if (registered.contains(item.file))
        {
            result.ids.push_back(item.id);
            if (!item.error.empty())
                result.errors.push_back(item.location + ": " + item.error);
        }
    return result;
}
std::unordered_set<fs::path> PresetFiles::copySources(const fs::path &inputSource, const ResolveConflict &resolve,
                                                     PresetImportResult &result)
{
    std::vector<fs::path> files;
    std::unordered_set<fs::path> registered;
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
                registered.insert(std::move(destination));
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
    return registered;
}
bool PresetFiles::save(const fs::path &relative, const std::string &text, bool replace, std::string &id,
                       std::string &error)
{
    return writePreset(relative, text, replace, id, error) && scan(error);
}
bool PresetFiles::writePreset(const fs::path &relative, const std::string &text, bool replace, std::string &id,
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
        return true;
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

} // namespace synth1x::host
