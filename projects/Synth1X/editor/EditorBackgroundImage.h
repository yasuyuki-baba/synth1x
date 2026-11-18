#pragma once
#include "editor/EditorPalette.h"
#include <string>

namespace synth1x::editor::native
{
// Validate with the same decoder used by the native renderer before publishing.
bool validateBackgroundImage(const EditorPalette& palette,std::string& error);
}
