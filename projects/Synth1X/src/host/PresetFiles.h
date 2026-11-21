#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace synth1x::host
{
struct UserPreset
{
    std::string id, name, location, archiveEntry, text, error;
    std::filesystem::path file;
    int program = 0;
};
enum class FileConflict
{
    replace,
    keepBoth,
    skip
};
struct ConflictChoice
{
    FileConflict action = FileConflict::keepBoth;
    bool applyToAll = false;
};
using ResolveConflict = std::function<ConflictChoice(const std::filesystem::path &)>;
struct PresetImportResult
{
    std::vector<std::string> ids;
    std::vector<std::string> errors;
    std::size_t imported = 0, skipped = 0;
    bool catalogReady = false;
};
// File operations are non-RT. Archives remain intact and are never extracted.
class PresetFiles
{
  public:
    explicit PresetFiles(std::filesystem::path root);
    // A catalog scan supplies a counter to enable its disposable archive cache.
    bool scan(std::string &error, bool validateSources = true, std::size_t* reusedArchives = nullptr);
    PresetImportResult import(const std::filesystem::path &source, const ResolveConflict &resolve);
    bool save(const std::filesystem::path &relative, const std::string &text, bool replace, std::string &id,
              std::string &error);
    const std::vector<UserPreset> &entries() const { return items; }
    const std::filesystem::path &directory() const { return root; }
    // Recognize only actual legacy records, not folders merely named banks/tags/favorites.
    static bool isManagedBank(const std::filesystem::path& root, const std::filesystem::path& bank);
    static bool isManagedPatch(const std::filesystem::path& root, const std::filesystem::path& file);
    static std::filesystem::path defaultDirectory();
    static std::filesystem::path settingsDirectory();
    static std::filesystem::path configuredDirectory();
    static bool saveStandaloneState(const std::vector<uint8_t> &, std::string &error);
    static std::vector<uint8_t> loadStandaloneState();
    static bool configureDirectory(const std::filesystem::path &, std::string &error);

  private:
    friend class PatchCatalog;
    static bool createLibraryFile(const std::filesystem::path&, const std::string&, std::string& error);
    // The catalog refreshes after the write and uses its validated entries to report imports.
    std::unordered_set<std::filesystem::path> copySources(const std::filesystem::path &, const ResolveConflict &,
                                                          PresetImportResult &);
    bool writePreset(const std::filesystem::path &, const std::string &, bool replace, std::string &id,
                     std::string &error);
    std::filesystem::path root;
    std::vector<UserPreset> items;
};
} // namespace synth1x::host
