#include "EditorStyle.h"
#include "EditorControls.h"
#include "EditorControlTags.h"
#include "IVKeyboardControl.h"
#include "IPopupMenuControl.h"

namespace synth1x::editor::native
{
namespace {
int controlGroup(int parameter)
{
    for(const auto* panels:{&editorPanels(),&editorMidiPanels()})
        for(const auto& section:*panels)
            for(const auto& item:section.controls)if(item.parameter==parameter)return paletteGroup(section.title);
    return -1;
}
}
NativeAppearance controlAppearance(const IControl& control,const char* group)
{
    static const EditorPalette defaults;
    const auto* owner=dynamic_cast<PaletteOwner*>(const_cast<IControl&>(control).GetDelegate());
    return nativeAppearance(owner ? owner->currentPalette() : defaults,group ? paletteGroup(group) : controlGroup(control.GetParamIdx()));
}
void refreshPalette(IGraphics& graphics,const EditorPalette& palette)
{
    for(int i=0;i<graphics.NControls();++i) {
        auto* control=graphics.GetControl(i);
        if(control->GetTag()==paletteTag)continue;
        const auto appearance=nativeAppearance(palette,controlGroup(control->GetParamIdx()));
        if(auto* vector=dynamic_cast<IVectorBase*>(control)) {
            auto style=vector->GetStyle();
            style=style.WithColor(kBG,appearance.panel).WithColor(kFG,appearance.control)
                .WithColor(kPR,appearance.accent).WithColor(kFR,appearance.border).WithColor(kHL,appearance.accent);
            style.labelText=style.labelText.WithFGColor(appearance.text);
            style.valueText=style.valueText.WithFGColor(appearance.text);
            if(dynamic_cast<PanelKnobControl*>(control)) {
                style=style.WithColor(kBG,COLOR_TRANSPARENT);
                if(editorGroupHeading(control->GetParamIdx()))
                    style.labelText=style.labelText.WithFont("Synth1XUIBold").WithFGColor(appearance.groupText);
            }
            if(control->GetTag()==presetTag)style=style.WithColor(kBG,appearance.display).WithColor(kFG,appearance.display);
            if(control->GetTag()==comparisonTag)style=style.WithColor(kPR,appearance.selectedRow);
            vector->SetStyle(style);
        }
        if(auto* keyboard=dynamic_cast<IVKeyboardControl*>(control)) {
            keyboard->SetColors(appearance.control,appearance.keyboardWhite,appearance.accent,appearance.border);
            keyboard->SetText(keyboard->GetText().WithFGColor(appearance.text));
        }
    }
    const auto appearance=nativeAppearance(palette);
    if(auto* popup=graphics.GetPopupMenuControl()) {
        popup->SetText(popup->GetText().WithFGColor(appearance.text));
        popup->SetPanelColor(appearance.panel);popup->SetCellBackgroundColor(appearance.control);
        popup->SetItemColor(appearance.text);popup->SetDisabledItemColor(appearance.muted);
        popup->SetItemMouseoverColor(appearance.selectedRow);popup->SetSeparatorColor(appearance.border);
    }
    // Translucent layers must be composited from the opaque canvas on every edit.
    graphics.SetAllControlsDirty();
}
}
