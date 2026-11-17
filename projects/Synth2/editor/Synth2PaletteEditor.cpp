#include "../Synth2.h"
#include "EditorPaletteWindow.h"
#include "EditorStyle.h"
#include "host/EditorPaletteStore.h"
#include "host/PresetFiles.h"

using namespace synth2::editor::native;

void Synth2::loadPalettePreferences()
{
    try {
        palettePreferenceError.clear();
        if(!synth2::host::EditorPaletteStore::load(synth2::host::PresetFiles::settingsDirectory()/"palette.xml",editorPalette,palettePreferenceError))
            browserStatus=palettePreferenceError;
    } catch(const std::exception& exception){palettePreferenceError=exception.what();browserStatus=palettePreferenceError;}
}
void Synth2::showPaletteEditor()
{
    if(paletteWindow){paletteWindow->raise();return;}
    auto* graphics=GetUI();if(!graphics)return;
    releaseComputerNotes();
    auto window=std::make_shared<EditorPaletteWindow>(editorPalette);
    if(!window->open(graphics->GetWindow())) {
        browserStatus="Could not open the palette editor.";refreshBrowser();return;
    }
    paletteEdit.begin(editorPalette);paletteWindow=std::move(window);
    if(!palettePreferenceError.empty())paletteWindow->reportError(palettePreferenceError);
}
void Synth2::updatePaletteEditor()
{
    if(!paletteWindow)return;
    const auto update=paletteWindow->poll();if(!update)return;
#ifdef OS_LINUX
    std::lock_guard lock(GfxMutex());
#endif
    if(update->action==EditorPaletteWindow::Action::cancel) {
        paletteEdit.cancel(editorPalette);paletteWindow.reset();
    } else {
        editorPalette=update->palette;
        if(update->action==EditorPaletteWindow::Action::apply) {
            std::string error;
            try {
                if(synth2::host::EditorPaletteStore::save(synth2::host::PresetFiles::settingsDirectory()/"palette.xml",editorPalette,error)) {
                    palettePreferenceError.clear();paletteEdit.apply();paletteWindow.reset();
                } else paletteWindow->reportError(error);
            } catch(const std::exception& exception){paletteWindow->reportError(exception.what());}
        }
    }
    if(auto* graphics=GetUI())refreshPalette(*graphics,editorPalette);
}
void Synth2::closePaletteEditor()
{
    if(!paletteWindow)return;
    paletteEdit.cancel(editorPalette);paletteWindow.reset();
    if(auto* graphics=GetUI())refreshPalette(*graphics,editorPalette);
}
