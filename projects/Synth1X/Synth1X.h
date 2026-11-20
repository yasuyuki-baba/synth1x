#pragma once
#include "IPlug_include_in_plug_hdr.h"
#include "ProcessorBridge.h"
#include "PatchCatalog.h"
#include "host/PatchComparison.h"
#include "editor/ComputerKeyboard.h"
#include "editor/EditorLibraryFilters.h"
#include "editor/EditorLibraryStatus.h"
#include "editor/EditorPalette.h"
#include <atomic>
#include <array>
#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>

namespace synth1x::editor::native {class EditorPaletteWindow;}

class Synth1X final : public iplug::Plugin, public synth1x::editor::PaletteOwner
{
public:
    const synth1x::editor::EditorPalette& currentPalette() const override {return editorPalette;}
    ~Synth1X() override;
    explicit Synth1X(const iplug::InstanceInfo& info);
    void ProcessBlock(iplug::sample**, iplug::sample** outputs, int frames) override;
    void ProcessMidiMsg(const iplug::IMidiMsg& message) override;
    void OnReset() override;
    void OnActivate(bool active) override;
    void OnParamChange(int index) override;
    void OnParamChange(int index, iplug::EParamSource source, int sampleOffset = -1) override;
    void OnParamReset(iplug::EParamSource source) override;
    bool SerializeState(iplug::IByteChunk& chunk) const override;
    int UnserializeState(const iplug::IByteChunk& chunk, int start) override;
    void OnIdle() override;
    void OnUIOpen() override;
    void OnParentWindowResize(int width,int height) override;
    void OnUIClose() override;

private:
    void loadPalettePreferences();
    void showPaletteEditor();
    void updatePaletteEditor();
    void closePaletteEditor();
    std::string palettePreferenceError;
    synth1x::editor::EditorPalette editorPalette;
    synth1x::editor::EditorPaletteEdit paletteEdit;
    std::shared_ptr<synth1x::editor::native::EditorPaletteWindow> paletteWindow;
    friend struct Synth1XVst3PatchTest;
    static constexpr int qualityParameter=synth1x::host::ProcessorBridge::parameterCount;
    void syncParametersFromProcessor();
    void syncQualityLatency();
    void refreshOversampling();
    void refreshDelayTiming();
    void showOversamplingMenu(iplug::igraphics::IControl* caller);
    void requestEditorScale(float scale);
    void reconcileEditorWindowSize();
    void refreshEditorScale();
    float editorScale=1.f;
    bool reconcileEditorScale=false;
    int displayedOversampling=0;
    std::unique_ptr<iplug::igraphics::IPopupMenu> qualityMenu;
    void layoutEditor(iplug::igraphics::IGraphics* graphics);
    void showPage(int page);
    void setKeyboardVisible(bool visible);
    void sendComputerNote(int note,int velocity);
    void releaseComputerNotes();
    void releaseKeyboardNotes();
    void updateKeyboardInput();
    synth1x::editor::ComputerKeyboard computerKeyboard;
    bool keyboardVisible = false;
    bool resizingParentWindow = false;
    int observedParentWidth=0,observedParentHeight=0;
    void refreshBrowser();
    void showPresetMenu(iplug::igraphics::IControl* caller);
    void promptForBank();
    void promptForFolder();
    void showImportMenu(iplug::igraphics::IControl* caller);
    void importBank(const std::string& path);
    void reloadCatalog(bool queueIfBusy = false);
    void showImportConflict();
    void pollLibraryJob();
    void stepBank(int direction);
    void selectBank(const std::string& id);
    void toggleTagFilter(const std::string& tag);
    void removeLibraryFilter(const synth1x::editor::EditorLibraryFilter& filter);
    void showFilterMenu(iplug::igraphics::IControl* caller,
                        const std::vector<synth1x::editor::EditorLibraryFilter>& filters,
                        const iplug::igraphics::IRECT& bounds);
    void showBankMenu(iplug::igraphics::IControl* caller);
    void showScrollableLibraryMenu(iplug::igraphics::IControl* caller,
                                   const iplug::igraphics::IRECT& bounds);
    void showAutomaticTagMenu(iplug::igraphics::IControl* caller);
    void showTagMenu(iplug::igraphics::IControl* caller);
    bool handleLibraryKey(const iplug::IKeyPress& key,bool up);
    void changeFilter(const char* query);
    void loadLibraryPatch(std::size_t index);
    void stepPatch(int direction);
    void notifyHostOfPatch();
    bool selectComparison(int slot);
    bool copyComparison(int source);
    enum class ComparisonAction { select, copy };
    bool changeComparison(int slot, ComparisonAction action);
    void savePreset(bool saveAs);
    void showSaveMenu(iplug::igraphics::IControl*);
    void writePreset(const std::filesystem::path&, bool confirm, const std::string& title = {});
    synth1x::host::PresetBrowserState captureBrowser() const;
    void restoreBrowser();
    synth1x::host::PresetBrowserState browser;
    bool lastModified = false;
    std::atomic<uint64_t> browserGeneration {0};
    struct ImportPrompt {
        std::mutex mutex;
        std::condition_variable changed;
        bool alive = true, pending = false, answered = false;
        std::filesystem::path path;
        synth1x::host::ConflictChoice choice;
    };
    struct LibraryJob {
        synth1x::host::PatchCatalog catalog;
        synth1x::host::PresetImportResult imported;
        std::string error;
        uint64_t generation = 0;
        std::string savedId, savedName;
        std::array<double,synth1x::host::ProcessorBridge::parameterCount> savedBaseline{};
    };
    void applyLibraryJob(LibraryJob result);
    std::future<LibraryJob> libraryJob;
    synth1x::editor::LibraryOperation libraryOperation=synth1x::editor::LibraryOperation::scan;
    std::shared_ptr<ImportPrompt> importPrompt;
    bool conflictShown = false;
    std::unique_ptr<iplug::igraphics::IPopupMenu> libraryMenu, conflictMenu, saveMenu;
    synth1x::host::ProcessorBridge processor;
    synth1x::host::PatchComparison comparison;
    bool prepared = false;
    synth1x::host::PatchCatalog catalog {synth1x::host::PatchCatalog::userDirectory()};
    synth1x::host::CatalogFilter libraryFilter;
    bool catalogLoaded = false;
    bool catalogReloadPending = false;
    std::string selectedPatchId,keyboardPatchId;
    double resultOffset = 0;
    int page = 0;
    std::string currentPatch = "Init";
    std::string browserStatus;
    std::unique_ptr<iplug::igraphics::IPopupMenu> presetMenu;
    std::shared_ptr<int> dialogLifetime;
    std::atomic<bool> stateRestored {false};
};
