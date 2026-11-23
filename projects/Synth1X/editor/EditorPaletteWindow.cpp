#include "EditorPaletteWindow.h"
#include "EditorPaletteShell.h"
#include "EditorBackgroundImage.h"
#include "editor/EditorFonts.h"
#include "host/EditorPaletteStore.h"
#include "IGraphics.h"
#include "IControls.h"
#include "ITextEntryControl.h"
#include "IPopupMenuControl.h"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace synth1x::editor::native
{
using namespace iplug;
using namespace iplug::igraphics;
namespace {
constexpr int width=EditorPaletteWindow::width,height=EditorPaletteWindow::height;
// Presentation order stays independent of enum values and stored palette keys.
constexpr std::array<PaletteRole,paletteRoleCount> paletteDisplayOrder {
    PaletteRole::background,PaletteRole::border,
    PaletteRole::header,PaletteRole::headerEdge,
    PaletteRole::panel,PaletteRole::groupPanel,
    PaletteRole::text,PaletteRole::muted,
    PaletteRole::heading,PaletteRole::groupText,
    PaletteRole::control,PaletteRole::controlEdge,
    PaletteRole::track,PaletteRole::pointer,
    PaletteRole::accent,PaletteRole::selectedRow,
    PaletteRole::display,PaletteRole::displayText,
    PaletteRole::keyboardWhite
};
const IColor canvas(255,30,30,34),field(255,47,47,52),line(255,95,91,96),text(255,240,235,226),muted(255,184,177,177);
IColor rgba(EditorColor color){return {color.alpha,color.red,color.green,color.blue};}
void drawSwatch(IGraphics& g,const IRECT& bounds,EditorColor value)
{
    constexpr float cell=8.f;
    for(float y=bounds.T;y<bounds.B;y+=cell)for(float x=bounds.L;x<bounds.R;x+=cell) {
        const auto odd=(static_cast<int>((x-bounds.L)/cell)+static_cast<int>((y-bounds.T)/cell))%2;
        g.FillRect(odd ? IColor(255,150,150,150) : IColor(255,210,210,210),
            IRECT(x,y,std::min(x+cell,bounds.R),std::min(y+cell,bounds.B)));
    }
    g.FillRect(rgba(value),bounds);g.DrawRect(line,bounds);
}
class PaletteEditorControl final : public IControl
{
public:
    PaletteEditorControl(EditorPaletteWindow& owner,const EditorPalette& palette)
        : IControl(IRECT(0,0,width,height)),owner(owner),draft(palette) {editing.begin(palette);}
    void requestCancel() {
        if(confirming() || fileDialogOpen)return;
        // Include valid text that is still being edited when the frame closes.
        if(auto* entry=GetUI()->GetTextEntryControl();entry && entry->EditInProgress())entry->CommitEdit();
        if(auto* popup=GetUI()->GetPopupMenuControl();popup && popup->GetExpanded())popup->OnMouseDown(-1.f,-1.f,IMouseMod{});
        drag=-1;
        if(editing.modified(draft)){confirmDiscard=true;SetDirty(false);}
        else owner.submit(EditorPaletteWindow::Action::cancel,draft);
    }
    bool handleKey(const IKeyPress& key,bool up) {
        if(!confirming())return fileDialogOpen;
        // Escape and Return dismiss confirmations without changing colors or files.
        if(!up && (key.VK==27 || key.VK==13)){confirmDiscard=false;confirmDefaults=false;exportReplacement.reset();SetDirty(false);}
        return true;
    }
    void setError(std::string value){error=std::move(value);notice.clear();SetDirty(false);}
    void Draw(IGraphics& g) override
    {
        const IText normal(14,text,"Synth1XUI",EAlign::Near);
        g.FillRect(canvas,mRECT);
        g.DrawText(normal.WithSize(20).WithFont("Synth1XUIBold"),"Palette",IRECT(24,14,876,46));
        g.DrawText(normal.WithFGColor(muted).WithSize(12),"Compare all colors on the left. Select one to edit; changes preview in the main editor.",IRECT(24,46,876,68));
        const auto box=[&](const IRECT& bounds,const char* label) {
            g.FillRoundRect(field,bounds,4);g.DrawRoundRect(line,bounds,4,nullptr,.5f);
            g.DrawText(normal,label,bounds.GetPadded(-10));
        };
        box(scopeBounds(),group<0 ? "General" : paletteGroupLabels[group]);
        const auto arrowX=scopeBounds().R-16,arrowY=scopeBounds().MH();
        g.FillTriangle(muted,arrowX-4,arrowY-2,arrowX+4,arrowY-2,arrowX,arrowY+3);
        for(std::size_t i=0;i<paletteDisplayOrder.size();++i) {
            const auto item=paletteDisplayOrder[i];const auto index=static_cast<std::size_t>(item);
            const bool editable=group<0 || paletteGroupRole(item);
            const auto bounds=colorBounds(i);
            const bool selected=item==role;
            g.FillRoundRect(selected ? IColor(255,63,48,55) : field,bounds,4);
            g.DrawRoundRect(selected ? IColor(255,237,139,136) : line,bounds,4,nullptr,selected ? 1.5f : .5f);
            const auto value=draft.resolve(item,group);
            drawSwatch(g,IRECT(bounds.L+7,bounds.T+6,bounds.L+39,bounds.B-6),value);
            g.DrawText(normal.WithSize(13),paletteRoleLabels[index],IRECT(bounds.L+49,bounds.T+3,bounds.R-6,bounds.T+21));
            g.DrawText(normal.WithSize(11).WithFGColor(muted),value.hex().c_str(),IRECT(bounds.L+49,bounds.T+21,bounds.R-72,bounds.B-2));
            if(!editable)g.FillRoundRect(IColor(130,30,30,34),bounds,4);
            else if(group>=0 && draft.groups[group][index])
                g.DrawText(normal.WithSize(11).WithAlign(EAlign::Far),"Custom",
                    IRECT(bounds.R-70,bounds.T+21,bounds.R-7,bounds.B-2));
        }
        g.DrawLine(line,500,134,500,588,nullptr,.5f);
        g.DrawText(normal.WithSize(18).WithFont("Synth1XUIBold"),paletteRoleLabels[static_cast<std::size_t>(role)],IRECT(524,134,876,166));
        const auto value=draft.resolve(role,group);
        drawSwatch(g,swatchBounds(),value);
        box(hexBounds(),value.hex().c_str());
        g.DrawText(normal.WithSize(12).WithFGColor(muted),"HEX / RGBA",IRECT(624,224,876,246));
        g.DrawText(normal.WithSize(12).WithFGColor(muted),"Click the large swatch to open the color picker.",IRECT(524,264,876,288));
        const int channels[]={value.red,value.green,value.blue,static_cast<int>(std::lround(value.alpha*100./255))};
        const char* labels[]={"Red","Green","Blue","Opacity"};
        for(int channel=0;channel<4;++channel) {
            const auto bounds=sliderBounds(channel);const int maximum=channel==3 ? 100 : 255;
            g.DrawText(normal,labels[channel],IRECT(524,bounds.T-8,602,bounds.B+8));
            g.FillRoundRect(field,bounds,4);
            const float x=bounds.L+channels[channel]*bounds.W()/maximum;
            g.FillRoundRect(line,IRECT(bounds.L,bounds.T,x,bounds.B),4);
            g.FillCircle(text,x,bounds.MH(),5);
            const auto number=std::to_string(channels[channel])+(channel==3 ? "%" : "");
            box(numberBounds(channel),number.c_str());
        }
        if(group>=0) {
            const bool inherited=!draft.groups[group][static_cast<std::size_t>(role)];
            g.DrawText(normal.WithSize(12).WithFGColor(muted),inherited ? "This color inherits from General." : "This color overrides General.",IRECT(524,446,876,470));
            box(inheritBounds(),"Use General");box(inheritGroupBounds(),"Reset group");
        } else g.DrawText(normal.WithSize(12).WithFGColor(muted),role==PaletteRole::background ? "Background is always opaque." : "Groups inherit these colors unless overridden.",IRECT(524,446,876,470));
        g.DrawText(normal.WithSize(12).WithFGColor(muted),"Background image (all groups)",IRECT(524,532,876,552));
        g.DrawText(normal.WithSize(12),draft.backgroundImage ? draft.backgroundImage->name.c_str() : "None",IRECT(524,552,876,574));
        box(imageBounds(),"Choose image...");box(removeImageBounds(),"Remove image");
        g.DrawText(normal.WithSize(12).WithFGColor(muted),"Below the header; panels can be transparent.",IRECT(524,614,876,636));
        box(defaultBounds(),"Default colors");box(importBounds(),"Import...");box(exportBounds(),"Export...");
        box(cancelBounds(),"Cancel");box(applyBounds(),"Apply");
        if(!error.empty())g.DrawMultiLineText(normal.WithSize(12).WithFGColor(IColor(255,255,176,166)),error.c_str(),IRECT(524,642,876,686));
        else if(!notice.empty())g.DrawMultiLineText(normal.WithSize(12).WithFGColor(muted),notice.c_str(),IRECT(524,642,876,686));
        if(confirming()) {
            const bool exporting=exportReplacement.has_value();
            const bool resetting=confirmDefaults;
            g.FillRect(IColor(190,0,0,0),mRECT);
            const IRECT dialog(200,224,700,436);
            g.FillRoundRect(canvas,dialog,6);g.DrawRoundRect(line,dialog,6,nullptr,1.f);
            g.DrawText(normal.WithSize(20).WithFont("Synth1XUIBold"),exporting ? "Replace exported palette?" : resetting ? "Restore default colors?" : "Discard palette changes?",IRECT(224,242,676,276));
            g.DrawMultiLineText(normal,exporting ? "A file with this name already exists.\nReplace it with the exported palette?" : resetting ? "Reset all General colors and group overrides?\nThis replaces your current palette edits." : "Your changes have not been saved.\nDiscard them and restore the previous colors?",IRECT(224,286,676,354));
            box(keepEditingBounds(),exporting ? "Cancel export" : resetting ? "Cancel" : "Keep editing");
            box(discardBounds(),exporting ? "Replace file" : resetting ? "Restore defaults" : "Discard changes");
            g.DrawRoundRect(text,keepEditingBounds(),4,nullptr,1.f);
        }
    }
    void OnMouseDown(float x,float y,const IMouseMod&) override
    {
        if(fileDialogOpen)return;
        if(confirming()) {
            if(keepEditingBounds().Contains(x,y)){confirmDiscard=false;confirmDefaults=false;exportReplacement.reset();}
            else if(discardBounds().Contains(x,y)) {
                if(exportReplacement) {
                    const auto replacement=std::move(*exportReplacement);exportReplacement.reset();
                    writeExport(replacement.path,replacement.palette);
                } else if(confirmDefaults) {
                    confirmDefaults=false;draft.resetColors();changed();
                } else owner.submit(EditorPaletteWindow::Action::cancel,draft);
            }
            SetDirty(false);return;
        }
        error.clear();notice.clear();drag=-1;
        if(scopeBounds().Contains(x,y)) {
            menu.Clear();menu.AddItem("General")->SetChecked(group<0);
            for(std::size_t i=0;i<paletteGroupLabels.size();++i)menu.AddItem(paletteGroupLabels[i])->SetChecked(group==static_cast<int>(i));
            GetUI()->CreatePopupMenu(*this,menu,scopeBounds());
        } else if(const auto selected=roleAt(x,y)) {
            role=*selected;
        } else if(hexBounds().Contains(x,y)) {
            const auto hex=draft.resolve(role,group).hex();
            beginEntry(hexBounds(),hex.c_str(),1);
        } else if(swatchBounds().Contains(x,y)) {
            picked=rgba(draft.resolve(role,group));
            const auto selectedRole=role;const int selectedGroup=group;
            const std::weak_ptr<int> weak=lifetime;
            GetUI()->PromptForColor(picked,"Palette color",[this,weak,selectedRole,selectedGroup](const IColor& chosen) {
                if(weak.expired())return;
                auto value=draft.resolve(selectedRole,selectedGroup);
                value.red=chosen.R;value.green=chosen.G;value.blue=chosen.B;value.alpha=chosen.A;
                draft.set(selectedRole,value,selectedGroup);changed();
            });
        } else if(defaultBounds().Contains(x,y)){confirmDefaults=!draft.hasDefaultColors();}
        else if(imageBounds().Contains(x,y))promptForImage();
        else if(removeImageBounds().Contains(x,y)){if(draft.backgroundImage){draft.backgroundImage.reset();changed();}}
        else if(importBounds().Contains(x,y))promptForPalette(false);
        else if(exportBounds().Contains(x,y))promptForPalette(true);
        else if(cancelBounds().Contains(x,y))requestCancel();
        else if(applyBounds().Contains(x,y))owner.submit(EditorPaletteWindow::Action::apply,draft);
        else if(group>=0 && inheritBounds().Contains(x,y)){draft.groups[group][static_cast<std::size_t>(role)].reset();changed();}
        else if(group>=0 && inheritGroupBounds().Contains(x,y)){draft.inherit(group);changed();}
        else for(int channel=0;channel<4;++channel) {
            if(sliderBounds(channel).GetPadded(8).Contains(x,y)) {drag=channel;slide(x);break;}
            if(numberBounds(channel).Contains(x,y)) {
                const auto value=draft.resolve(role,group);
                const int number=channel==0 ? value.red : channel==1 ? value.green : channel==2 ? value.blue : static_cast<int>(std::lround(value.alpha*100./255));
                beginEntry(numberBounds(channel),std::to_string(number).c_str(),channel+2);break;
            }
        }
        SetDirty(false);
    }
    void OnMouseDrag(float x,float,float,float,const IMouseMod&) override {if(!confirming() && !fileDialogOpen && drag>=0)slide(x);}
    void OnMouseUp(float,float,const IMouseMod&) override {drag=-1;}
    void OnTextEntryCompletion(const char* input,int index) override
    {
        std::string_view value=input ? input : "";
        if(index==1) {
            if(auto parsed=EditorColor::parse(value)) {draft.set(entryRole,*parsed,entryGroup);changed();}
            else error="Use #RRGGBB or #RRGGBBAA.";
        } else if(index>=2 && index<=5) {
            if(value.ends_with('%'))value.remove_suffix(1);
            int number=0;const auto result=std::from_chars(value.data(),value.data()+value.size(),number);
            const int maximum=index==5 ? 100 : 255;
            if(result.ec==std::errc{} && result.ptr==value.data()+value.size() && number>=0 && number<=maximum)
                setChannel(index-2,index==5 ? static_cast<int>(std::lround(number*255./100)) : number,entryRole,entryGroup);
            else error=index==5 ? "Opacity must be 0–100%." : "RGB values must be 0–255.";
        }
        SetDirty(false);
    }
    void OnPopupMenuSelection(IPopupMenu* selection,int) override
    {
        if(!selection)return;const int index=selection->GetChosenItemIdx();if(index<0)return;
        if(index>static_cast<int>(paletteGroupKeys.size()))return;
        group=index-1;
        if(group>=0 && !paletteGroupRole(role))role=PaletteRole::panel;
        SetDirty(false);
    }
private:
    IRECT keepEditingBounds() const{return {224,376,444,412};}
    IRECT discardBounds() const{return {456,376,676,412};}
    IRECT scopeBounds() const{return {24,78,356,114};}
    IRECT colorBounds(std::size_t index) const {
        const auto x=24.f+static_cast<float>(index%2)*236.f;
        const auto row=index/2;
        const auto sectionGap=(row>=3 ? 6.f : 0.f)+(row>=5 ? 6.f : 0.f)+(row>=8 ? 6.f : 0.f);
        const auto y=134.f+static_cast<float>(row)*44.f+sectionGap;
        return {x,y,x+224,y+40};
    }
    std::optional<PaletteRole> roleAt(float x,float y) const {
        // Scope changes must never move cells; non-group roles keep their slots.
        for(std::size_t i=0;i<paletteDisplayOrder.size();++i) {
            const auto item=paletteDisplayOrder[i];
            if((group<0 || paletteGroupRole(item)) && colorBounds(i).Contains(x,y))return item;
        }
        return {};
    }
    IRECT swatchBounds() const{return {524,182,604,254};}
    IRECT hexBounds() const{return {624,182,876,218};}
    IRECT sliderBounds(int channel) const{return {612.f,310.f+channel*36,794.f,320.f+channel*36};}
    IRECT numberBounds(int channel) const{return {812.f,300.f+channel*36,876.f,330.f+channel*36};}
    IRECT inheritBounds() const{return {524,486,690,520};}
    IRECT inheritGroupBounds() const{return {704,486,876,520};}
    IRECT defaultBounds() const{return {24,696,180,732};}
    IRECT importBounds() const{return {196,696,308,732};}
    IRECT exportBounds() const{return {320,696,432,732};}
    IRECT cancelBounds() const{return {640,696,752,732};}
    IRECT applyBounds() const{return {764,696,876,732};}
    IRECT imageBounds() const{return {524,578,706,610};}
    IRECT removeImageBounds() const{return {718,578,876,610};}
    bool confirming() const {return confirmDiscard || confirmDefaults || exportReplacement.has_value();}
    void writeExport(const std::filesystem::path& path,const EditorPalette& palette) {
        if(host::EditorPaletteStore::save(path,palette,error))notice="Palette exported.";
        SetDirty(false);
    }
    void promptForPalette(bool exporting) {
        if(auto* entry=GetUI()->GetTextEntryControl();entry && entry->EditInProgress())entry->CommitEdit();
        WDL_String file(exporting ? "Synth1X-palette.xml" : ""),directory(lastDirectory.c_str());
        const std::weak_ptr<int> weak=lifetime;
        fileDialogOpen=true;
        GetUI()->PromptForFile(file,directory,exporting ? EFileAction::Save : EFileAction::Open,"xml",
            [this,weak,exporting,snapshot=draft](const WDL_String& chosen,const WDL_String& folder) {
                if(weak.expired())return;
                fileDialogOpen=false;
                if(!chosen.GetLength()){SetDirty(false);return;}
                lastDirectory=folder.Get();
                try {
                    auto destination=std::filesystem::u8path(chosen.Get(),chosen.Get()+chosen.GetLength());
                    if(exporting) {
                        const bool extensionAdded=destination.extension().empty();
                        if(extensionAdded)destination+=".xml";
#ifdef OS_LINUX
                        // The pinned GTK save dialog does not confirm replacements.
                        const bool confirmReplacement=true;
#else
                        // Native save dialogs confirm the selected name, but not
                        // a different target produced by adding an extension here.
                        const bool confirmReplacement=extensionAdded;
#endif
                        if(confirmReplacement && std::filesystem::exists(destination))
                            exportReplacement=ExportReplacement{destination,snapshot};
                        else writeExport(destination,snapshot);
                    } else {
                        auto next=draft;
                        if(host::EditorPaletteStore::importFile(destination,next,error) && validateBackgroundImage(next,error)) {
                            draft=std::move(next);changed();notice="Palette imported. Apply to save.";
                        }
                    }
                } catch(const std::exception&) {
                    error=exporting ? "Could not export the palette. Check the file and folder permissions."
                                    : "Could not import the palette. Check the selected file and permissions.";
                }
                SetDirty(false);
            });
    }
    void promptForImage() {
        WDL_String file,directory(lastDirectory.c_str());
        const std::weak_ptr<int> weak=lifetime;fileDialogOpen=true;
        GetUI()->PromptForFile(file,directory,EFileAction::Open,"png jpg jpeg",
            [this,weak](const WDL_String& chosen,const WDL_String& folder) {
                if(weak.expired())return;
                fileDialogOpen=false;
                if(!chosen.GetLength()){SetDirty(false);return;}
                lastDirectory=folder.Get();
                try {
                    auto next=draft;
                    if(host::EditorPaletteStore::readImage(std::filesystem::u8path(chosen.Get(),chosen.Get()+chosen.GetLength()),next,error) &&
                       validateBackgroundImage(next,error)){draft=std::move(next);changed();}
                } catch(const std::exception&){error="Could not open the background image.";}
                SetDirty(false);
            });
    }
    void beginEntry(const IRECT& bounds,const char* value,int index) {
        entryRole=role;entryGroup=group;
        GetUI()->CreateTextEntry(*this,IText(14,text,"Synth1XUI"),bounds,value,index);
    }
    void changed(){error.clear();notice.clear();owner.submit(EditorPaletteWindow::Action::preview,draft);SetDirty(false);}
    void setChannel(int channel,int number,PaletteRole selectedRole,int selectedGroup) {
        auto value=draft.resolve(selectedRole,selectedGroup);
        if(channel==0)value.red=number;if(channel==1)value.green=number;if(channel==2)value.blue=number;if(channel==3)value.alpha=number;
        draft.set(selectedRole,value,selectedGroup);changed();
    }
    void slide(float x) {
        const auto bounds=sliderBounds(drag);const int maximum=drag==3 ? 100 : 255;
        const int number=static_cast<int>(std::lround(std::clamp((x-bounds.L)/bounds.W(),0.f,1.f)*maximum));
        setChannel(drag,drag==3 ? static_cast<int>(std::lround(number*255./100)) : number,role,group);
    }
    EditorPaletteWindow& owner;
    EditorPalette draft;
    EditorPaletteEdit editing;
    bool confirmDiscard=false,confirmDefaults=false,fileDialogOpen=false;
    struct ExportReplacement {std::filesystem::path path;EditorPalette palette;};
    std::optional<ExportReplacement> exportReplacement;
    int group=-1,drag=-1;
    PaletteRole role=PaletteRole::panel;
    PaletteRole entryRole=PaletteRole::panel;
    int entryGroup=-1;
    IPopupMenu menu;
    std::string error,notice,lastDirectory;
    IColor picked;
    std::shared_ptr<int> lifetime=std::make_shared<int>(0);
};
}
EditorPaletteWindow::EditorPaletteWindow(const EditorPalette& palette)
    : IGEditorDelegate(0),shell(std::make_unique<EditorPaletteShell>()),initial(palette)
{
    mMakeGraphicsFunc=[this]{return makePaletteGraphics(*this);};
    mLayoutFunc=[this](IGraphics* graphics) {
        if(graphics->NControls())return;
        const auto regular=editorRegularFont(),bold=editorBoldFont();
        graphics->LoadFont("Synth1XUI",regular.data,regular.size);graphics->LoadFont("Synth1XUIBold",bold.data,bold.size);
        graphics->EnableMouseOver(true);graphics->AttachTextEntryControl();
        graphics->AttachPopupMenuControl(IText(14,text,"Synth1XUI"));
        auto* control=new PaletteEditorControl(*this,initial);
        graphics->AttachControl(control);
        graphics->SetKeyHandlerFunc([control](const IKeyPress& key,bool up){return control->handleKey(key,up);});
    };
}
EditorPaletteWindow::~EditorPaletteWindow(){CloseWindow();}
bool EditorPaletteWindow::open(void* owner)
{
    if(!shell->open(owner,width,height))return false;
    auto* view=OpenWindow(shell->parent());if(!view)return false;
    shell->attach(view);return true;
}
void EditorPaletteWindow::raise(){shell->raise();}
void EditorPaletteWindow::submit(Action action,const EditorPalette& palette)
{
    std::lock_guard lock(updatesMutex);pending=Update{action,palette};
}
std::optional<EditorPaletteWindow::Update> EditorPaletteWindow::poll()
{
    if(shell->takeCloseRequest()) {
        if(auto* graphics=GetUI())
            if(auto* control=dynamic_cast<PaletteEditorControl*>(graphics->GetControl(0)))control->requestCancel();
    }
    std::lock_guard lock(updatesMutex);auto update=std::move(pending);pending.reset();return update;
}
void EditorPaletteWindow::reportError(const std::string& error)
{
    if(auto* graphics=GetUI())if(auto* control=dynamic_cast<PaletteEditorControl*>(graphics->GetControl(0)))control->setError(error);
}
}
