#pragma once
#include "editor/EditorLibraryInteraction.h"
#include "editor/EditorLibraryFilters.h"
#include "IControls.h"
#include "editor/EditorLayout.h"
#include "editor/EditorPalette.h"
#include "editor/EditorPresetDisplay.h"
#include "host/PatchCatalog.h"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace synth1x::editor::native
{
using namespace iplug;
using namespace iplug::igraphics;

// Compose the SDK momentary-button lifecycle with application callbacks.
class ActionButtonControl : public IVButtonControl
{
public:
    ActionButtonControl(const IRECT& bounds, IActionFunction action, const char* label,
                        const IVStyle& style);
};

// A single clickable display with independent title and bank lines.
class PresetDisplayControl final : public ActionButtonControl
{
public:
    using ActionButtonControl::ActionButtonControl;
    void setDisplay(const synth1x::editor::EditorPresetDisplay& display);
    void Draw(IGraphics& graphics) override;
private:
    synth1x::editor::EditorPresetDisplay display;
};

// Preserve text roles even when two palette colors happen to be identical.
class PaletteTextControl final : public ITextControl
{
public:
    PaletteTextControl(const IRECT& bounds,const char* label,const IText& text,
                       PaletteRole role=PaletteRole::text,const char* group=nullptr)
        : ITextControl(bounds,label,text),role(role),group(group) {}
    void Draw(IGraphics& graphics) override;
private:
    PaletteRole role;
    const char* group;
};

// Covers the complete editor after the optional keyboard changes its height.
class BackgroundControl final : public IPanelControl
{
public:
    explicit BackgroundControl(const IRECT& bounds);
    void OnResize() override;
    void Draw(IGraphics& graphics) override;
private:
    std::shared_ptr<const EditorBackgroundImage> image;
    IBitmap bitmap;
};

class HeaderControl final : public IControl
{
public:
    static constexpr float height=118.f;
    HeaderControl();
    void Draw(IGraphics& graphics) override;
};

class SectionControl final : public IControl
{
public:
    explicit SectionControl(const EditorPanel& section);
    void Draw(IGraphics& graphics) override;
private:
    const EditorPanel& section;
};

class PanelKnobControl final : public IVKnobControl
{
public:
    PanelKnobControl(const IRECT& bounds,int parameter,const char* label,const IVStyle& style);
    void OnResize() override;
    void DrawValue(IGraphics& graphics,bool mouseOver) override;
    void DrawHandle(IGraphics& graphics,const IRECT& bounds) override;
    void DrawPointer(IGraphics& graphics,float angle,float cx,float cy,float radius) override;
    void DrawIndicatorTrack(IGraphics& graphics,float angle,float cx,float cy,float radius) override;
};

// Selectors retain original source words and their host automation ranges.
class SourceChoiceControl final : public IControl
{
public:
    SourceChoiceControl(const IRECT& bounds,int parameter,const char* label,std::function<double()> sourceValue,std::function<void(int)> submitSource = {});
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& modifiers) override;
    void OnPopupMenuSelection(IPopupMenu* selection,int value) override;
private:
    std::function<double()> sourceValue;
    std::function<void(int)> submitSource;
    const char* label;
    bool toggle;
    std::vector<EditorChoice> choices;
    IPopupMenu menu;
};

// Borrow the catalog/selection; draw only rows intersecting the scroll viewport.
class CatalogListControl final : public IControl
{
public:
    CatalogListControl(const IRECT& bounds,const host::PatchCatalog& catalog,
        const std::string& selected,const std::string& keyboardSelection,std::function<void(std::size_t)> load,
        std::function<void(const std::string&,bool)> favorite,
        std::function<void(const std::string&)> bank,std::function<void(const std::string&)> tag,
        std::function<void(double)> scroll,std::function<void(const char*)> import);
    void setResults(std::vector<std::size_t> results,double start);
    void setEmptyMessage(const std::string& title,const std::string& detail);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseDrag(float x,float y,float dX,float dY,const IMouseMod& modifiers) override;
    void OnMouseUp(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseWheel(float x,float y,const IMouseMod& modifiers,float delta) override;
    void OnMouseOver(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseOut() override;
    void OnDrop(const char* path) override;
    void OnDropMultiple(const std::vector<const char*>& paths) override;
private:
    const host::PatchCatalog& catalog;
    const std::string& selected;
    const std::string& keyboardSelection;
    std::vector<std::size_t> matches;
    std::string emptyTitle,emptyDetail;
    double offset=0,drawOffset=0,animationStart=0;
    bool draggingScroll=false;
    float scrollGrab=0,hoverX=-1,hoverY=-1;
    std::vector<EditorLibraryTag> rowTags(const host::CatalogPatch& item) const;
    std::optional<std::size_t> rowAt(float y) const;
    EditorLibraryTarget targetAt(float x,const host::CatalogPatch& item) const;
    IRECT scrollTrack() const;
    IRECT scrollThumb() const;
    void dragScroll(float y);
    std::function<void(std::size_t)> load;
    std::function<void(const std::string&,bool)> favorite;
    std::function<void(const std::string&)> filterBank,filterTag;
    std::function<void(double)> scroll;
    std::function<void(const char*)> import;
};

// Compact removal buttons; the native popup exposes conditions that do not fit.
class LibraryFiltersControl final : public IControl
{
public:
    LibraryFiltersControl(const IRECT& bounds,
        std::function<void(const EditorLibraryFilter&)> remove,
        std::function<void(IControl*,const std::vector<EditorLibraryFilter>&,const IRECT&)> more);
    void setFilters(std::vector<EditorLibraryFilter> filters);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseOver(float x,float y,const IMouseMod& modifiers) override;
    void OnMouseOut() override;
private:
    std::vector<EditorLibraryFilterChip> chips() const;
    std::vector<EditorLibraryFilter> filters;
    float hoverX=-1;
    std::function<void(const EditorLibraryFilter&)> remove;
    std::function<void(IControl*,const std::vector<EditorLibraryFilter>&,const IRECT&)> more;
};

class SearchControl final : public IEditableTextControl
{
public:
    SearchControl(const IRECT& bounds,const char* text,const IText& style,
                  std::function<void(const char*)> change,const char* placeholder="");
    void Draw(IGraphics& graphics) override;
    void OnTextEntryCompletion(const char* text,int value) override;
private:
    std::function<void(const char*)> onChange;
    std::string placeholder;
};

class FooterChoiceControl final : public IControl
{
public:
    FooterChoiceControl(const IRECT& bounds,const char* label,float labelWidth,
                        const char* text,IActionFunction action);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& mod) override;
    void setText(const char* text);
private:
    std::string label,text;
    float labelWidth;
    IActionFunction action;
};

// Click the value to type a percentage; the arrow opens the preset menu.
class DisplayScaleControl final : public IControl
{
public:
    DisplayScaleControl(const IRECT& bounds,std::function<void(float)> change);
    void Draw(IGraphics& graphics) override;
    void OnMouseDown(float x,float y,const IMouseMod& mod) override;
    void OnMouseDblClick(float x,float y,const IMouseMod& mod) override;
    void OnTextEntryCompletion(const char* text,int value) override;
    void OnPopupMenuSelection(IPopupMenu* selection,int value) override;
    void setScale(float scale);
private:
    IRECT valueBounds() const;
    std::function<void(float)> onChange;
    IPopupMenu menu;
    std::string text="100%";
    float scale=1.f;
};
}
