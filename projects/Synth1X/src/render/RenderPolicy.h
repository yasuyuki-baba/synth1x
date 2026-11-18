#pragma once

namespace synth1x { struct RenderControls; }

namespace synth1x::render
{
    // The loading adapter supplies control meanings, never a file document,
    // source version, preset identity, APVTS object or indexed parameter array.
    struct RenderPolicyInputs
    {
        bool effectControlActive {},stackControlActive {};
        bool effectEnabled {};
        int effectType {};
        int stackDial {},subLevelDial {};
        int modulationAmountDial {64},modulationAttackDial {},modulationDecayDial {};
    };

    // Compose common controls after source conversion and value mapping.
    void selectRenderPolicy(const RenderPolicyInputs&,RenderControls& target);
}
