#pragma once
#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace synth1x::editor
{
struct EditorChoice { int value; const char* label; const char* group=nullptr; };
struct EditorBounds { int x,y,width,height; };
struct EditorControl { int parameter; const char* label; };
inline bool editorGroupHeading(int parameter)
{
    return parameter==0 || parameter==1 || parameter==96 || parameter==45 || parameter==6 || parameter==7;
}
inline bool editorOscillatorGrouped(int parameter)
{
    switch(parameter) {
        case 0: case 76: case 45: case 95: case 96: case 97:
        case 1: case 2: case 3: case 4: case 6: case 7: return true;
        default: return false;
    }
}
inline const char* editorControlHelp(int parameter)
{
    switch(parameter) {
        case 4:return "OSC2 keyboard tracking. When off, OSC2 uses a fixed pitch.";
        case 5:return "OSC1/OSC2 balance; Sub follows the OSC1 side. With Ring on, blends dry OSC1 and the ring-modulated signal.";
        case 6:return "OSC1 synchronizes OSC2 by resetting its cycle. OSC2 Pitch changes the synced timbre.";
        case 7:return "Ring-modulates OSC1 with OSC2. Mix blends dry OSC1 and the ring-modulated signal. Ring takes precedence over FM.";
        case 8:return "Pulse width for OSC1 and OSC2 when a pulse waveform is selected.";
        case 45:return "OSC2 modulates OSC1. Ring takes precedence over FM processing; the FM amount is retained and can still affect Ring-mode level.";
        case 91:return "Oscillator start phase: 0 is random; fixed settings establish OSC1/OSC2 phase relationships at note-on.";
        case 92:return "Phase spread across Unison voices when a fixed oscillator start phase is selected.";
        case 95:return "Sub oscillator level. Sub follows OSC1 pitch, and the OSC1/OSC2 Mix also affects its level.";
        case 96:return "Sub oscillator waveform, referenced to OSC1 pitch and the Sub octave setting.";
        case 97:return "Sub oscillator pitch relative to OSC1: the same octave or one octave lower.";
        default:return nullptr;
    }
}
struct EditorPanel
{
    const char* title;
    const char* description;
    const char* accent;
    EditorBounds bounds;
    int columns;
    std::vector<EditorControl> controls;
};
inline bool editorToggle(int index)
{
    switch(index)
    {
        case 4: case 6: case 7: case 10: case 57: case 58: case 59: case 65: case 66:
        case 67: case 68: case 69: case 70: case 73: case 74: case 77: return true;
        default: return false;
    }
}
// Source integers from Synth1PatchIoCodec.cpp, not indices into a new host
// enumeration. The host's source ranges and saved values stay intact.
inline std::vector<EditorChoice> editorChoices(int index)
{
    switch(index)
    {
        case 0: return {{0,"Sine"},{1,"Saw"},{2,"Pulse"},{3,"Triangle"}};
        case 1: return {{1,"Saw"},{2,"Pulse"},{3,"Triangle"},{4,"Noise"}};
        case 14: return {{0,"LP12"},{1,"LP24"},{2,"HP12"},{3,"BP12"},{4,"Diode"}};
        case 31: return {{1,"Up / down"},{2,"Up"},{3,"Down"},{4,"Random"}};
        case 32: return {{0,"1 oct"},{1,"2 oct"},{2,"3 oct"},{3,"4 oct"}};
        case 33: return {{0,"1/1"},{1,"1/2+1/4+1/8"},{2,"1/2+1/4"},{3,"1/2"},{4,"1/4+1/8+1/16"},
                         {5,"1/4+1/8"},{6,"1/1 triplet"},{7,"1/4"},{8,"1/8+1/16+1/32"},{9,"1/8+1/16"},
                         {10,"1/2 triplet"},{11,"1/8"},{12,"1/16+1/32"},{13,"1/4 triplet"},{14,"1/16"},
                         {15,"1/8 triplet"},{16,"1/32"},{17,"1/16 triplet"},{18,"1/32 triplet"}};
        case 35: return {{1,"1/64 triplet"},{2,"1/32 triplet"},{3,"1/32"},{4,"1/16 triplet"},
                         {5,"1/16"},{6,"1/8 triplet"},{7,"1/16 dotted"},{8,"1/8"},
                         {9,"1/4 triplet"},{10,"1/8 dotted"},{11,"1/8+1/16+1/32"},{12,"1/4"},
                         {13,"1/2 triplet"},{14,"1/4 dotted"},{15,"1/4+1/8+1/16"},{16,"1/2"},
                         {17,"1/2 dotted"},{18,"1/2+1/4+1/8"},{19,"1/1"}};
        case 38: return {{0,"Poly"},{1,"Mono"},{2,"Legato"}};
        case 40: return {{0,"0 st"},{1,"1 st"},{2,"2 st"},{3,"3 st"},{4,"4 st"},
            {5,"5 st"},{6,"6 st"},{7,"7 st"},{8,"8 st"},{9,"9 st"},{10,"10 st"},
            {11,"11 st"},{12,"12 st"},{13,"13 st"},{14,"14 st"},{15,"15 st"},
            {16,"16 st"},{17,"17 st"},{18,"18 st"},{19,"19 st"},{20,"20 st"},
            {21,"21 st"},{22,"22 st"},{23,"23 st"},{24,"24 st"}};
        case 41: case 46: return {{1,"OSC2"},{2,"OSC1 + 2"},{3,"Filter"},{4,"Amp"},{5,"Width"},{6,"FM"},{7,"Pan"}};
        case 42: case 47: return {{0,"Saw"},{1,"Triangle"},{2,"Square"},{3,"Random S&H"},{4,"Random smooth"},{5,"Sine"}};
        case 64: return {{1,"1x"},{2,"2x"},{4,"4x"}};
        case 71: return {{0,"OSC2"},{1,"FM"},{2,"Width"}};
        case 78: return {{0,"Analog Distortion 1"},{1,"Analog Distortion 2"},{2,"Digital Distortion"},{3,"Decimator"},{4,"Ring Modulator"},
                         {5,"Compressor"},{6,"Phaser (2 stage)"},{7,"Phaser (4 stage)"},
                         {8,"Phaser (8 stage)"},{9,"Phaser (12 stage)"}};
        case 82: return {{0,"Stereo"},{1,"Cross"},{2,"Ping-pong"}};
        case 86: case 88: return {
            {45057,"CC1 - Mod Wheel"},{45058,"CC2 - Breath"},{45059,"CC3"},
            {45060,"CC4 - Foot Controller"},{45061,"CC5 - Portamento Time"},
            {45062,"CC6 - Data Entry"},{45063,"CC7 - Volume"},{45064,"CC8 - Balance"},
            {45065,"CC9"},{45066,"CC10 - Pan"},{45067,"CC11 - Expression"},
            {53248,"Channel Aftertouch"},{57344,"Pitch Bend"}};
        case 87: case 89: return {
            {-1,"None"},{76,"OSC1 Detune","Oscillators"},{45,"OSC1 FM","Oscillators"},{95,"Sub Level","Oscillators"},{2,"OSC2 Pitch","Oscillators"},
            {12,"Mod Env Attack","Oscillators"},{13,"Mod Env Decay","Oscillators"},{11,"Mod Env Amount","Oscillators"},{5,"Oscillator Mix","Oscillators"},
            {8,"Pulse Width","Oscillators"},{91,"Oscillator Phase","Oscillators"},{15,"Filter Attack","Filter"},{16,"Filter Decay","Filter"},
            {17,"Filter Sustain","Filter"},{18,"Filter Release","Filter"},{19,"Filter Cutoff","Filter"},{20,"Filter Resonance","Filter"},
            {21,"Filter Env Depth","Filter"},{23,"Filter Drive","Filter"},{22,"Filter Key Track","Filter"},
            {25,"Amp Attack","Amplifier"},{26,"Amp Decay","Amplifier"},{27,"Amp Sustain","Amplifier"},{28,"Amp Release","Amplifier"},{29,"Amp Level","Amplifier"},
            {79,"Effect Control 1","Effect"},{80,"Effect Control 2","Effect"},{81,"Effect Output","Effect"},
            {61,"EQ Frequency","EQ"},{62,"EQ Level","EQ"},{63,"EQ Q","EQ"},{60,"EQ Tone","EQ"},{90,"Pan","Output"},
            {35,"Delay Time","Delay"},{83,"Delay Spread","Delay"},{36,"Delay Feedback","Delay"},{98,"Delay Tone","Delay"},{37,"Delay Mix","Delay"},
            {52,"Chorus Time","Chorus"},{53,"Chorus Depth","Chorus"},{54,"Chorus Rate","Chorus"},{55,"Chorus Feedback","Chorus"},{56,"Chorus Level","Chorus"},
            {39,"Glide Time","Voice"},{75,"Unison Detune","Voice"},{92,"Unison Phase","Voice"},{84,"Unison Spread","Voice"},{85,"Unison Pitch","Voice"},
            {34,"Arp Gate","Arpeggiator"},{43,"LFO1 Rate","LFO"},{44,"LFO1 Amount","LFO"},{48,"LFO2 Rate","LFO"},{49,"LFO2 Amount","LFO"}};
        case 96: return {{0,"Sine"},{1,"Triangle"},{2,"Saw"},{3,"Pulse"}};
        case 97: return {{0,"Same"},{1,"-1 oct"}};
        default: return {};
    }
}

inline const std::vector<EditorPanel>& editorPanels()
{
    static const std::vector<EditorPanel> panels {
        {"OSCILLATORS","OSC1 + OSC2  /  FM, Ring & Sub","#d9aaa2",{20,124,548,320},6,
            {{0,"OSC 1"},{76,"Detune"},{1,"OSC 2"},{2,"Pitch"},{3,"Fine"},{4,"Track"},
             {96,"SUB"},{97,"Octave"},{95,"Level"},
             {45,"FM 2\xe2\x86\x92" "1"},{6,"Sync 1\xe2\x86\x92" "2"},{7,"Ring 1\xc3\x97" "2"},
             {10,"Mod env"},{71,"Env to"},{12,"Attack"},{13,"Decay"},{11,"Amount"},
             {5,"Mix 1/2"},{8,"Pulse width"},{9,"Key shift"},{72,"Fine 1+2"},{91,"Start phase"}}},
        {"FILTER","Tone & filter envelope","#ed8b88",{580,124,356,272},4,
            {{14,"Type"},{19,"Cutoff"},{20,"Resonance"},{23,"Drive"},{15,"Attack"},{16,"Decay"},
             {17,"Sustain"},{18,"Release"},{21,"Env depth"},{22,"Key track"},{24,"Velocity"}}},
        {"AMPLIFIER","Level & amplitude envelope","#d9aaa2",{580,404,356,200},4,
            {{25,"Attack"},{26,"Decay"},{27,"Sustain"},{28,"Release"},{29,"Level"},{30,"Velocity"}}},
        {"LFO 1","Independent modulation","#d9aaa2",{948,124,312,156},3,
            {{57,"Enabled"},{42,"Wave"},{41,"To"},{43,"Rate"},{44,"Amount"},{67,"Tempo"},{68,"Key sync"}}},
        {"LFO 2","Independent modulation","#d9aaa2",{948,288,312,156},3,
            {{58,"Enabled"},{47,"Wave"},{46,"To"},{48,"Rate"},{49,"Amount"},{69,"Tempo"},{70,"Key sync"}}},
        {"VOICE","Play mode, polyphony & glide","#d9aaa2",{20,452,208,152},2,
            {{38,"Mode"},{94,"Polyphony"},{39,"Glide"},{74,"Auto glide"}}},
        {"UNISON","Voice stacking & stereo spread","#d9aaa2",{236,452,332,152},6,
            {{73,"Enabled"},{93,"Voices"},{75,"Detune"},{84,"Spread"},{85,"Pitch"},{92,"Phase"}}},
        {"ARPEGGIATOR","Tempo-based note motion","#d9aaa2",{948,452,312,152},3,
            {{59,"Enabled"},{31,"Pattern"},{32,"Range"},{33,"Rate"},{34,"Gate"}}},
        {"EFFECT","Drive & character","#d9aaa2",{20,612,292,178},3,
            {{77,"Enabled"},{78,"Type"},{81,"Output"},{79,"Control 1"},{80,"Control 2"}}},
        {"CHORUS","Width & movement","#d9aaa2",{324,612,300,178},4,
            {{66,"Enabled"},{64,"Voices"},{52,"Time"},{53,"Depth"},{54,"Rate"},{55,"Feedback"},{56,"Level"}}},
        {"DELAY","Space & repeats","#d9aaa2",{636,612,392,178},4,
            {{65,"Enabled"},{82,"Type"},{35,"Time"},{36,"Feedback"},{37,"Mix"},{83,"Spread"},{98,"Tone"}}},
        {"EQ","Equalizer and tone shaping","#d9aaa2",{1040,612,220,178},2,
            {{61,"Frequency"},{62,"Level"},{63,"Q"},{60,"Tone"}}}
    };
    return panels;
}

inline EditorBounds editorControlBounds(const EditorPanel& panel,std::size_t cell)
{
    if (std::string(panel.title)=="OSCILLATORS")
    {
        // Main oscillators, then Sub/interactions, modulation and shared settings.
        const int row=cell<12 ? static_cast<int>(cell)/6 : cell<17 ? 2 : 3;
        const int columns=row<2 ? 6 : 5;
        const int column=static_cast<int>(cell)-(row<2 ? row*6 : row==2 ? 12 : 17);
        const int width=(panel.bounds.width-16)/columns;
        const int top=row<2 ? row*72 : row==2 ? 142 : 212;
        const int inset=row<2 ? 4 : 0;
        return {panel.bounds.x+8+column*width+inset,panel.bounds.y+38+top,width-2*inset,68};
    }
    if (std::string(panel.title)=="AMPLIFIER")
    {
        const int width=(panel.bounds.width-16)/(cell<4 ? 4 : 2);
        const int height=(panel.bounds.height-44)/2;
        return {panel.bounds.x+8+static_cast<int>(cell<4 ? cell : cell-4)*width,
                panel.bounds.y+38+(cell<4 ? 0 : height),width,height};
    }
    if (std::string(panel.title)=="VOICE")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int width=(panel.bounds.width-16)/2;
        const int parameter=panel.controls[cell].parameter;
        if(parameter==38 || parameter==74)return {x+(parameter==74 ? width : 0),y,width,44};
        return {x+(parameter==39 ? width : 0),y+44,width,68};
    }
    if (std::string(panel.title)=="UNISON")
    {
        const int enabledWidth=64,width=(panel.bounds.width-16-enabledWidth)/5;
        return {panel.bounds.x+8+(cell==0 ? 0 : enabledWidth+static_cast<int>(cell-1)*width),
            panel.bounds.y+38+(panel.bounds.height-112)/2,cell==0 ? enabledWidth : width,68};
    }
    if (std::string(panel.title)=="ARPEGGIATOR")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int width=(panel.bounds.width-16)/3;
        if(cell==0)return {x,y,width,44};
        if(cell==1)return {x+width,y,width*2,44};
        return {x+static_cast<int>(cell-2)*width,y+44,width,68};
    }
    if (std::string(panel.title)=="FILTER")
    {
        const int x=panel.bounds.x,y=panel.bounds.y;
        const int parameter=panel.controls[cell].parameter;
        if (parameter==19) return {x+8,y+38,144,98};
        if (parameter==20) return {x+156,y+38,96,98};
        if (parameter==14) return {x+256,y+38,92,44};
        if (parameter==23) return {x+256,y+82,92,54};
        if (parameter>=15 && parameter<=18) return {x+8+(parameter-15)*85,y+136,85,64};
        const int column=parameter==21 ? 0 : parameter==22 ? 1 : 2;
        return {x+8+column*113,y+200,113,66};
    }
    if (std::string(panel.title)=="EFFECT")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int width=(panel.bounds.width-16)/3,height=(panel.bounds.height-44)/2;
        if (cell==0) return {x,y,width,height};
        if (cell==1) return {x+width,y,width*2,height};
        return {x+(cell==2 ? 2 : static_cast<int>(cell)-3)*width,y+height,width,height};
    }
    if (std::string(panel.title)=="LFO 1" || std::string(panel.title)=="LFO 2")
    {
        const int x=panel.bounds.x+8,y=panel.bounds.y+38;
        const int inner=panel.bounds.width-16;
        if(cell<3)
        {
            const int enabledWidth=60,choiceWidth=(inner-enabledWidth)/2;
            return {x+(cell==0 ? 0 : enabledWidth+static_cast<int>(cell-1)*choiceWidth),
                    y,cell==0 ? enabledWidth : choiceWidth,44};
        }
        const int width=inner/4;
        return {x+static_cast<int>(cell-3)*width,y+44,width,68};
    }
    if(std::string(panel.title)=="CHORUS" && cell>=4)
    {
        const int width=(panel.bounds.width-16)/3,height=(panel.bounds.height-44)/2;
        return {panel.bounds.x+8+static_cast<int>(cell-4)*width,panel.bounds.y+38+height,width,height};
    }
    const int rows=(static_cast<int>(panel.controls.size())+panel.columns-1)/panel.columns;
    const int width=(panel.bounds.width-16)/panel.columns;
    const int height=(panel.bounds.height-44)/rows;
    // Keep Time's hit area above the timing readout while retaining its choice height.
    if(std::string(panel.title)=="DELAY" && panel.controls[cell].parameter==35)
        return {panel.bounds.x+8+2*width,panel.bounds.y+38,width,52};
    // Reserve the cell below Time for tempo and actual stereo delay times.
    if(std::string(panel.title)=="DELAY" && panel.controls[cell].parameter==98)
        return {panel.bounds.x+8+3*width,panel.bounds.y+38+height,width,height};
    return {panel.bounds.x+8+static_cast<int>(cell%panel.columns)*width,
            panel.bounds.y+38+static_cast<int>(cell/panel.columns)*height,width,height};
}
inline std::vector<EditorBounds> editorPanelDividers(const EditorPanel& panel)
{
    const int x=panel.bounds.x,y=panel.bounds.y;
    if(std::string(panel.title)=="OSCILLATORS")
    {
        return {{x+8,y+249,panel.bounds.width-16,1}};
    }
    if(std::string(panel.title)=="AMPLIFIER")
        return {{x+12,y+38+(panel.bounds.height-44)/2-6,panel.bounds.width-24,1}};
    if(std::string(panel.title)=="MIDI")
        return {{x+160,y+38,1,panel.bounds.height-50},
                {x+184,y+116,panel.bounds.width-200,1}};
    return {};
}
inline std::vector<EditorBounds> editorPanelGroups(const EditorPanel& panel)
{
    if(std::string(panel.title)!="OSCILLATORS")return {};
    const int x=panel.bounds.x+6,y=panel.bounds.y+36;
    const int inner=panel.bounds.width-12,cell=(panel.bounds.width-16)/6;
    const int osc1=2*cell+4,sub=3*cell+4;
    return {{x,y,osc1,72},{x+osc1,y,inner-osc1,72},
            {x,y+72,sub,72},{x+sub,y+72,inner-sub,72}};
}
inline const std::vector<EditorPanel>& editorMidiPanels()
{
    static const std::vector<EditorPanel> panels {
        {"MIDI","Pitch bend and two controller assignments saved with each sound","#d9aaa2",{20,124,1000,216},3,
            {{40,"Bend range"},{86,"Input 1"},{50,"Sensitivity"},{87,"Destination"},
             {88,"Input 2"},{51,"Sensitivity"},{89,"Destination"}}}
    };
    return panels;
}
inline EditorBounds editorMidiControlBounds(const EditorPanel& panel,std::size_t cell)
{
    const int x=panel.bounds.x,y=panel.bounds.y;
    if(cell==0)return {x+12,y+78,128,72};
    const int row=static_cast<int>(cell-1)/3,column=static_cast<int>(cell-1)%3;
    const int top=y+38+row*84;
    if(column==0)return {x+210,top,280,72};
    if(column==1)return {x+506,top,96,72};
    return {x+618,top,366,72};
}
inline constexpr EditorBounds editorDelayTimingBounds {840,702,78,51};
inline constexpr EditorControl editorMasterVolumeControl {128,"Master volume"};
inline constexpr EditorControl editorPanControl {90,"Pan"};
inline constexpr EditorBounds editorMasterVolumeBounds {1020,12,116,68};
inline constexpr EditorBounds editorPanBounds {1144,12,116,68};
inline constexpr EditorBounds editorPresetDisplayBounds {240,16,638,62};
inline constexpr EditorBounds editorComparisonBounds {546,82,72,30};
inline constexpr EditorBounds editorCopyAToBBounds {628,82,52,30};
inline constexpr EditorBounds editorCopyBToABounds {688,82,52,30};
// Persistent controls stay outside the Synth/MIDI/Library view groups.
inline constexpr EditorBounds editorDisplayScaleBounds {20,82,144,28};
inline constexpr EditorBounds editorLibrarySummaryBounds {250,82,284,30};
inline constexpr EditorBounds editorVersionBounds {700,802,224,28};
inline constexpr EditorBounds editorOversamplingBounds {940,802,148,28};
inline constexpr EditorBounds editorKeyboardToggleBounds {1100,802,160,28};
}
