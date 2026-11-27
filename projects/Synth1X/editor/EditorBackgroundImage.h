#pragma once
#include "editor/EditorPalette.h"
#include <string>

namespace synth1x::editor::native
{
// Decode and validate before publishing, independently of the drawing backend.
bool validateBackgroundImage(const EditorPalette& palette,std::string& error);
}
