#pragma once
#include "editor/EditorPalette.h"
#include <filesystem>
#include <string>

namespace synth1x::host
{
// UI-thread preferences only. Never serialized into patches or processor state.
struct EditorPaletteStore
{
    static bool load(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error);
    // Explicit imports reject missing files; preference loading uses defaults.
    static bool importFile(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error);
    static bool readImage(const std::filesystem::path& path,editor::EditorPalette& palette,std::string& error);
    static bool save(const std::filesystem::path& path,const editor::EditorPalette& palette,std::string& error);
};
}
