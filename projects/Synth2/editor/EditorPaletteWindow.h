#pragma once
#include "editor/EditorPalette.h"
#include "IGraphicsEditorDelegate.h"
#include <memory>
#include <mutex>
#include <optional>

namespace synth2::editor::native
{
class EditorPaletteShell;
iplug::igraphics::IGraphics* makePaletteGraphics(iplug::igraphics::IGEditorDelegate& delegate);
class EditorPaletteWindow final : public iplug::igraphics::IGEditorDelegate
{
public:
    static constexpr int width=900,height=756;
    enum class Action {preview,apply,cancel};
    struct Update {Action action;EditorPalette palette;};
    explicit EditorPaletteWindow(const EditorPalette& palette);
    ~EditorPaletteWindow();
    bool open(void* owner);
    void raise();
    std::optional<Update> poll();
    void reportError(const std::string& error);
    void submit(Action action,const EditorPalette& palette);
    void BeginInformHostOfParamChangeFromUI(int) override {}
    void EndInformHostOfParamChangeFromUI(int) override {}
private:
    std::unique_ptr<EditorPaletteShell> shell;
    EditorPalette initial;
    std::mutex updatesMutex;
    std::optional<Update> pending;
};
}
