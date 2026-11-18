#include "editor/EditorLayout.h"
#include "BuildVersion.h"
#include "editor/EditorPages.h"
#include "editor/EditorAppearance.h"
#include "editor/EditorTypography.h"
#include "editor/EditorValueDisplay.h"
#include "editor/ParameterPresentation.h"
#include "host/ProcessorBridge.h"
#include "patch/Synth1SourceRange.h"
#include <iomanip>
#include <iostream>
#include <cmath>

// SDK-independent manifest for the interactive layout preview. The native
// editor and preview consume the same panel geometry and source-choice values.
int main()
{
    synth1x::host::ProcessorBridge processor;
    const auto& a=synth1x::editor::editorAppearance();
    std::cout << "{\"width\":1280,\"height\":920,\"version\":" << std::quoted(SYNTH1X_VERSION_STR)
              << ",\"chrome\":{";
    bool firstBounds=true;
    for(const auto& [name,b]:std::initializer_list<std::pair<const char*,synth1x::editor::EditorBounds>>{
        {"displayScale",synth1x::editor::editorDisplayScaleBounds},
        {"librarySummary",synth1x::editor::editorLibrarySummaryBounds},
        {"presetDisplay",synth1x::editor::editorPresetDisplayBounds},
        {"masterVolume",synth1x::editor::editorMasterVolumeBounds},
        {"pan",synth1x::editor::editorPanBounds},
        {"comparison",synth1x::editor::editorComparisonBounds},
        {"copyAToB",synth1x::editor::editorCopyAToBBounds},
        {"copyBToA",synth1x::editor::editorCopyBToABounds},
        {"version",synth1x::editor::editorVersionBounds},
        {"oversampling",synth1x::editor::editorOversamplingBounds},
        {"keyboardToggle",synth1x::editor::editorKeyboardToggleBounds}})
    {
        if(!firstBounds)std::cout << ',';firstBounds=false;
        std::cout << std::quoted(name) << ":[" << b.x << ',' << b.y << ',' << b.width << ',' << b.height << ']';
    }
    std::cout << "},\"appearance\":{";
    bool firstColor=true;
    for (const auto& color:std::initializer_list<std::pair<const char*,const char*>>{
        {"background",a.background},{"header",a.header},{"headerEdge",a.headerEdge},{"panel",a.panel},{"selectedRow",a.selectedRow},
        {"filterPanel",a.filterPanel},{"groupPanel",a.groupPanel},{"groupText",a.groupText},
        {"border",a.border},{"text",a.text},{"muted",a.muted},{"accent",a.accent},
        {"display",a.display},{"displayText",a.displayText},{"control",a.control},{"controlEdge",a.controlEdge},
        {"track",a.track},{"pointer",a.pointer}})
    {
        if (!firstColor) std::cout << ','; firstColor=false;
        std::cout << std::quoted(color.first) << ':' << std::quoted(color.second);
    }
    using T=synth1x::editor::EditorTypography;
    std::cout << "},\"typography\":{\"auxiliary\":" << T::auxiliary
              << ",\"label\":" << T::label << ",\"value\":" << T::value
              << ",\"button\":" << T::button << ",\"heading\":" << T::heading
              << ",\"patchTitle\":" << T::patchTitle << "},\"panels\":[";
    const auto writePanel=[&](const synth1x::editor::EditorPanel& panel,bool midi) {
        std::cout << "{\"title\":" << std::quoted(panel.title) << ",\"description\":" << std::quoted(panel.description)
                  << ",\"accent\":" << std::quoted(panel.accent) << ",\"bounds\":[" << panel.bounds.x << ','
                  << panel.bounds.y << ',' << panel.bounds.width << ',' << panel.bounds.height << "],\"dividers\":[";
        bool firstDivider=true;
        for(const auto& b:synth1x::editor::editorPanelDividers(panel))
        {
            if(!firstDivider)std::cout << ',';firstDivider=false;
            std::cout << '[' << b.x << ',' << b.y << ',' << b.width << ',' << b.height << ']';
        }
        std::cout << "],\"groups\":[";
        bool firstGroup=true;
        for(const auto& b:synth1x::editor::editorPanelGroups(panel)) {
            if(!firstGroup)std::cout << ',';firstGroup=false;
            std::cout << '[' << b.x << ',' << b.y << ',' << b.width << ',' << b.height << ']';
        }
        std::cout << "],\"controls\":[";
        bool first=true;
        for (std::size_t cell=0;cell<panel.controls.size();++cell)
        {
            if (!first) std::cout << ','; first=false;
            const auto& control=panel.controls[cell];
            const auto b=midi ? synth1x::editor::editorMidiControlBounds(panel,cell) : synth1x::editor::editorControlBounds(panel,cell);
            std::cout << "{\"parameter\":" << control.parameter << ",\"label\":" << std::quoted(control.label)
                      << ",\"bounds\":[" << b.x << ',' << b.y << ',' << b.width << ',' << b.height << "]"
                      << ",\"diameter\":" << synth1x::editor::editorKnobDiameter(control.parameter,b.height)
                      << ",\"groupLabel\":" << (synth1x::editor::editorGroupHeading(control.parameter) ? "true" : "false")
                      << ",\"help\":" << std::quoted(synth1x::editor::editorControlHelp(control.parameter) ? synth1x::editor::editorControlHelp(control.parameter) : "")
                      << ",\"tone\":" << std::quoted(control.parameter==19 ? a.accent : a.pointer) << '}';
        }
        std::cout << "]}";
    };
    bool firstPanel=true;
    for(const auto& panel:synth1x::editor::editorPanels()) {
        if(!firstPanel)std::cout << ',';firstPanel=false;writePanel(panel,false);
    }
    std::cout << "],\"midiPanels\":[";firstPanel=true;
    for(const auto& panel:synth1x::editor::editorMidiPanels()) {
        if(!firstPanel)std::cout << ',';firstPanel=false;writePanel(panel,true);
    }
    const auto writeHeaderControl=[&](const char* key,const synth1x::editor::EditorControl& control,const synth1x::editor::EditorBounds& bounds) {
        std::cout << "," << std::quoted(key) << ":{\"parameter\":" << control.parameter << ",\"label\":" << std::quoted(control.label)
                  << ",\"name\":" << std::quoted(processor.parameter(control.parameter).name)
                  << ",\"diameter\":" << synth1x::editor::editorKnobDiameter(control.parameter,bounds.height) << '}';
    };
    std::cout << ']';
    writeHeaderControl("headerMaster",synth1x::editor::editorMasterVolumeControl,synth1x::editor::editorMasterVolumeBounds);
    writeHeaderControl("headerPan",synth1x::editor::editorPanControl,synth1x::editor::editorPanBounds);
    const auto timing=synth1x::editor::editorDelayTimingBounds;
    std::cout << ",\"delayTimingBounds\":[" << timing.x << ',' << timing.y << ','
              << timing.width << ',' << timing.height << "],\"parameters\":[";
    for (int index=0;index<processor.parameterCount;++index)
    {
        if (index) std::cout << ',';
        const auto& info=processor.parameter(index);
        const auto initial=std::lround(index==87 || index==89 ? processor.midiDestination((index-87)/2)
            : info.minimum+processor.normalizedValue(index)*(info.maximum-info.minimum));
        std::cout << "{\"minimum\":" << (index==128 ? info.minimum : (index==87 || index==89) ? -1 : synth1x::sourceMinimum(index)) << ",\"maximum\":" << (index==128 ? info.maximum : synth1x::sourceMaximum(index))
                  << ",\"initial\":" << initial
                  << ",\"integerSteps\":" << (synth1x::editor::parameterUsesIntegerSteps(index) ? "true" : "false")
                  << ",\"toggle\":" << (synth1x::editor::editorToggle(index) ? "true" : "false") << ",\"choices\":[";
        bool first=true;
        for (const auto& choice:synth1x::editor::editorChoices(index))
        {
            if (!first) std::cout << ','; first=false;
            std::cout << '[' << choice.value << ',' << std::quoted(choice.label) << ',' << std::quoted(choice.group ? choice.group : "") << ']';
        }
        std::cout << "],\"unitDisplay\":[";
        if (synth1x::editor::editorHasUnit(index))
            for (int raw=0;raw<128;++raw)
            {
                if (raw) std::cout << ',';
                std::cout << std::quoted(synth1x::editor::editorValueDisplay(index,raw));
            }
        std::cout << "]}";
    }
    std::cout << "],\"supplements\":[";
    for (std::size_t i=5;i<synth1x::editor::editorPages().size();++i)
    {
        if (i!=5) std::cout << ',';
        const auto& page=synth1x::editor::editorPages()[i];
        std::cout << "{\"name\":" << std::quoted(page.name) << ",\"parameters\":[";
        bool first=true;
        for (int parameter:page.parameters) { if (!first) std::cout << ','; first=false; std::cout << parameter; }
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
