#pragma once
#include <memory>

namespace synth1x::editor::native
{
// Native owner window; iGraphics remains responsible for all editing controls.
class EditorPaletteShell
{
public:
    EditorPaletteShell();
    ~EditorPaletteShell();
    bool open(void* owner,int width,int height);
    void* parent() const;
    void attach(void* view);
    bool takeCloseRequest();
    void raise();
private:
    struct State;
    std::unique_ptr<State> state;
};
}
