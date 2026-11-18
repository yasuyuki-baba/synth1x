#pragma once
#include "editor/EditorPalette.h"
#include "editor/EditorTypography.h"
#include "editor/EditorLayout.h"
#include "IGraphicsStructs.h"
#include "IControl.h"

namespace synth1x::editor::native
{
using namespace iplug::igraphics;
using Typography=synth1x::editor::EditorTypography;
inline IRECT rect(const synth1x::editor::EditorBounds& b) { return IRECT(b.x,b.y,b.x+b.width,b.y+b.height); }
inline IColor color(EditorColor c) {return IColor(c.alpha,c.red,c.green,c.blue);}
inline IColor color(const char* hex) {return color(EditorColor::parse(hex).value_or(EditorColor{}));}
struct NativeAppearance
{
    IColor background;
    IColor header;
    IColor headerEdge;
    IColor panel;
    IColor groupPanel;
    IColor groupText;
    IColor border;
    IColor text;
    IColor muted;
    IColor accent;
    IColor display;
    IColor displayText;
    IColor selectedRow;
    IColor control;
    IColor controlEdge;
    IColor track;
    IColor pointer;
    IColor heading;
    IColor keyboardWhite;
};
inline NativeAppearance nativeAppearance(const EditorPalette& palette,int group=-1)
{
    return {color(palette.resolve(PaletteRole::background,group)),color(palette.resolve(PaletteRole::header,group)),color(palette.resolve(PaletteRole::headerEdge,group)),color(palette.resolve(PaletteRole::panel,group)),color(palette.resolve(PaletteRole::groupPanel,group)),color(palette.resolve(PaletteRole::groupText,group)),color(palette.resolve(PaletteRole::border,group)),color(palette.resolve(PaletteRole::text,group)),color(palette.resolve(PaletteRole::muted,group)),color(palette.resolve(PaletteRole::accent,group)),color(palette.resolve(PaletteRole::display,group)),color(palette.resolve(PaletteRole::displayText,group)),color(palette.resolve(PaletteRole::selectedRow,group)),color(palette.resolve(PaletteRole::control,group)),color(palette.resolve(PaletteRole::controlEdge,group)),color(palette.resolve(PaletteRole::track,group)),color(palette.resolve(PaletteRole::pointer,group)),color(palette.resolve(PaletteRole::heading,group)),color(palette.resolve(PaletteRole::keyboardWhite,group))};
}
NativeAppearance controlAppearance(const IControl& control,const char* group=nullptr);
void refreshPalette(IGraphics& graphics,const EditorPalette& palette);
}
