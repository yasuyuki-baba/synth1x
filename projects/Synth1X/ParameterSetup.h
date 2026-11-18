#pragma once
#include "IPlugParameter.h"
#include "src/editor/ParameterPresentation.h"
#include "src/host/ParameterInfo.h"
#include <cmath>

namespace synth1x::host
{
inline void initializeHostParameter(iplug::IParam& target,int index,const ParameterInfo& info)
{
    const auto label=info.id+" "+info.name;
    const double initial=info.minimum+info.defaultNormalized*(info.maximum-info.minimum);
    // Keep original IDs, source ranges and sparse selection values. InitEnum or
    // InitBool would replace those ranges and reinterpret saved automation.
    if (editor::parameterUsesIntegerSteps(index))
        target.InitInt(label.c_str(),static_cast<int>(std::lround(initial)),
                       static_cast<int>(info.minimum),static_cast<int>(info.maximum));
    else
        target.InitDouble(label.c_str(),initial,info.minimum,info.maximum,0.001);
    // MIDI and bend-range names are editor-only; retain numeric host metadata.
    // Expanding packed sources to a consecutive host display table would
    // allocate 65537 strings per source parameter.
    const auto choices=editor::editorChoices(index);
    if (index!=40 && (index<86 || index>89) && (editor::editorToggle(index) || !choices.empty()))
    {
        // AUv2 reads display strings by consecutive source value. A sparse
        // display table (e.g. chorus 1/2/4) would mislabel or omit those values.
        for(int value=static_cast<int>(info.minimum);value<=static_cast<int>(info.maximum);++value)
        {
            std::string text=value==0 ? "Default" : "Custom ("+std::to_string(value)+")";
            if(editor::editorToggle(index))
                text=value==0 ? "OFF" : value==1 ? "ON" : "ON ("+std::to_string(value)+")";
            for(const auto& choice:choices) if(choice.value==value) text=choice.label;
            target.SetDisplayText(value,text.c_str());
        }
    }
}
}
