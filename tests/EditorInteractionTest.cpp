#include "editor/EditorResize.h"
#include "editor/EditorDisplayScale.h"
#include "editor/EditorLibraryScroll.h"
#include "editor/EditorLibraryNavigation.h"
#include "editor/EditorLibraryInteraction.h"
#include "editor/EditorLibraryFilters.h"
#include "editor/EditorLibraryStatus.h"
#include <gtest/gtest.h>
#include "editor/EditorInteraction.h"
#include "editor/EditorValueDisplay.h"
#include "host/ProcessorBridge.h"
#include "SynthEngine.h"
#include "patch/PatchFormat.h"
#include "editor/ParameterPresentation.h"
#include <array>
#include <utility>

using namespace synth2::editor;

TEST(EditorLibraryStatus, FirstLoadDoesNotAppearEmptyAndFailureIsDistinctFromSuccess)
{
    EditorLibraryStatusInput input;input.busy=true;
    const auto loading=editorLibraryStatus(input);
    EXPECT_EQ(loading.summary,"Loading library...");
    EXPECT_EQ(loading.emptyTitle,"Loading library...");
    EXPECT_NE(loading.emptyDetail.find("automatic tags"),std::string::npos);
    EXPECT_EQ(loading.matches,"Loading library...");
    input.busy=false;
    const auto failed=editorLibraryStatus(input);
    EXPECT_EQ(failed.emptyTitle,"Library unavailable");
    EXPECT_NE(failed.emptyDetail.find("Refresh"),std::string::npos);
    input.ready=true;
    const auto empty=editorLibraryStatus(input);
    EXPECT_EQ(empty.summary,"0 sounds in library");
    EXPECT_EQ(empty.emptyTitle,"Your library is empty.");
    EXPECT_NE(empty.emptyDetail.find("Add..."),std::string::npos);
}

TEST(EditorLibraryStatus, RefreshRetainsKnownSizeAndReturnsToTheMatchingRange)
{
    EditorLibraryStatusInput input;
    input.ready=true;input.sounds=200;input.matches=50;input.offset=12.5;
    EXPECT_EQ(editorLibraryStatus(input).matches,"13 - 30 of 50 sounds");
    input.busy=true;
    const auto updating=editorLibraryStatus(input);
    EXPECT_EQ(updating.summary,"200 sounds / Updating library...");
    EXPECT_EQ(updating.matches,"Updating library...");
    input.busy=false;
    EXPECT_EQ(editorLibraryStatus(input).matches,"13 - 30 of 50 sounds");
    input.matches=0;
    const auto filtered=editorLibraryStatus(input);
    EXPECT_EQ(filtered.emptyTitle,"No matching sounds.");
    EXPECT_EQ(filtered.emptyDetail,"Clear a filter to see more sounds.");
    EXPECT_EQ(filtered.summary,"200 sounds in library");
}

TEST(EditorLibraryStatus, ImportAndSaveActivityEndsWhenTheJobFinishes)
{
    EditorLibraryStatusInput input;input.ready=true;input.busy=true;input.sounds=8;input.matches=8;
    input.operation=LibraryOperation::addSounds;
    EXPECT_EQ(editorLibraryStatus(input).matches,"Adding sounds...");
    input.operation=LibraryOperation::saveSound;
    EXPECT_EQ(editorLibraryStatus(input).matches,"Saving sound...");
    input.busy=false;input.registeredOnly=true;
    EXPECT_EQ(editorLibraryStatus(input).matches,"1 - 8 of 8 sounds");
    EXPECT_EQ(editorLibraryStatus(input).summary,"Registered / 8 sounds in library");
}

namespace
{
using synth2::host::ProcessorBridge;
void submitSource(ProcessorBridge& processor,int parameter,double source)
{
    if(parameter==87 || parameter==89) {
        ASSERT_TRUE(processor.setMidiDestination((parameter-87)/2,static_cast<int>(source)));return;
    }
    const auto& range=processor.parameter(parameter);
    processor.setNormalizedValue(parameter,(source-range.minimum)/(range.maximum-range.minimum));
}
double sourceValue(ProcessorBridge& processor,int parameter)
{
    if(parameter==87 || parameter==89)return processor.midiDestination((parameter-87)/2);
    const auto& range=processor.parameter(parameter);
    return range.minimum+processor.normalizedValue(parameter)*(range.maximum-range.minimum);
}
}

TEST(EditorInteraction, EveryPopupSelectionWritesItsSourceWordToOnlyTheBoundParameter)
{
    using namespace synth2::host;
    ProcessorBridge processor;
    for(int parameter=0;parameter<99;++parameter)
    {
        const auto choices=editorChoices(parameter);
        for(int selection=0;selection<static_cast<int>(choices.size());++selection)
        {
            SCOPED_TRACE(parameter);
            SCOPED_TRACE(selection);
            std::array<double,129> before{};
            for(int i=0;i<129;++i) before[i]=processor.normalizedValue(i);
            const auto source=editorSelectedSourceValue(choices,selection);
            ASSERT_TRUE(source); ASSERT_EQ(*source,choices[selection].value);
            submitSource(processor,parameter,*source);
            EXPECT_NEAR(sourceValue(processor,parameter),*source,1e-4);
            for(int i=0;i<129;++i) if(i!=parameter) EXPECT_EQ(processor.normalizedValue(i),before[i]);
            EXPECT_EQ(editorChoiceDisplay(false,sourceValue(processor,parameter),choices),choices[selection].label);
        }
    }
}

TEST(EditorInteraction, CancelledOrInvalidPopupSelectionsHaveNoSourceValue)
{
    using namespace synth2::host;
    for(int parameter=0;parameter<99;++parameter)
    {
        const auto choices=editorChoices(parameter);
        EXPECT_FALSE(editorSelectedSourceValue(choices,-1));
        EXPECT_FALSE(editorSelectedSourceValue(choices,static_cast<int>(choices.size())));
        EXPECT_FALSE(editorSelectedSourceValue(choices,999));
    }
}

TEST(EditorInteraction, EveryToggleHandlesHostCustomValuesAndCyclesOffOnOff)
{
    using namespace synth2::host;
    ProcessorBridge processor;
    for(int parameter=0;parameter<99;++parameter) if(editorToggle(parameter))
    {
        SCOPED_TRACE(parameter);
        submitSource(processor,parameter,63);
        for(int expected:{0,1,0})
        {
            submitSource(processor,parameter,editorToggledSourceValue(sourceValue(processor,parameter)));
            EXPECT_NEAR(sourceValue(processor,parameter),expected,1e-5);
            EXPECT_EQ(editorChoiceDisplay(true,sourceValue(processor,parameter),{}),expected ? "ON" : "OFF");
        }
    }
}

TEST(EditorInteraction, HostAutomationAndStateRecallReachChoiceAndNumericPresentation)
{
    using namespace synth2::host;
    ProcessorBridge processor,restored;
    submitSource(processor,64,4); // Chorus menu ordinal 2 is source word 4.
    submitSource(processor,9,-12);
    const auto saved=processor.saveState();
    restored.restoreState(saved.data(),static_cast<int>(saved.size()));
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,64),editorChoices(64)),"4x");
    EXPECT_EQ(editorValueDisplay(9,sourceValue(restored,9)),"-12");
    submitSource(restored,64,2);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,64),editorChoices(64)),"2x");
    EXPECT_EQ(editorChoiceDisplay(false,127,editorChoices(64)),"Custom");
}

TEST(EditorInteraction, EditingStepsDistinguishSelectorsCountsAndContinuousDials)
{
    using namespace synth2::host;
    for(int parameter : {0,1,4,14,31,33,35,38,40,64,86,87,88,89,93,94,96,126})
    { SCOPED_TRACE(parameter); EXPECT_TRUE(parameterUsesIntegerSteps(parameter)); }
    for(int parameter : {5,8,15,19,20,25,29,43,52,61,62,63,75,50,51,90,128})
    { SCOPED_TRACE(parameter); EXPECT_FALSE(parameterUsesIntegerSteps(parameter)); }
    // Sparse source words and the full legacy range are not replaced by ordinals.
    EXPECT_EQ(editorSelectedSourceValue(editorChoices(64),2),4);
    ProcessorBridge processor;
    EXPECT_EQ(processor.parameter(64).maximum,127);
    EXPECT_EQ(processor.parameter(86).minimum,-1);
    EXPECT_EQ(processor.parameter(86).maximum,65535);
    // Editing policy must not become a loading/state quantizer.
    submitSource(processor,64,3.25);
    const auto before=processor.saveState();
    ProcessorBridge restored;
    restored.restoreState(before.data(),static_cast<int>(before.size()));
    EXPECT_NEAR(sourceValue(restored,64),3.25,1e-4);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,64),editorChoices(64)),"Custom");
}

TEST(EditorInteraction, ContinuousUnitReadoutsKeepFractionalPositions)
{
    using namespace synth2::host;
    // Midpoint positions give geometric means for the logarithmic scales.
    EXPECT_EQ(editorValueDisplay(25,63.5),"118.3 ms");
    EXPECT_EQ(editorValueDisplay(28,63.5),"774.6 ms");
    EXPECT_EQ(editorValueDisplay(61,63.5),"894 Hz");
    EXPECT_EQ(editorValueDisplay(62,64.5),"0.2 dB");
    EXPECT_EQ(editorValueDisplay(63,63.5),"3.16 Q");
    EXPECT_EQ(editorValueDisplay(19,64.25),"64.25");
}

TEST(EditorLibraryNavigation, EmptyClampedAndPagedMovementNeverWraps)
{
    using namespace synth2::editor;using A=LibraryNavigation;
    EXPECT_FALSE(editorLibraryNavigation(0,16,{},0,A::next));
    EXPECT_EQ(editorLibraryNavigation(40,16,{},0,A::next),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,{},16,A::previous),16u);
    EXPECT_EQ(editorLibraryNavigation(40,16,0,0,A::previous),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,39,32,A::next),39u);
    EXPECT_EQ(editorLibraryNavigation(40,16,5,0,A::pageNext),21u);
    EXPECT_EQ(editorLibraryNavigation(40,16,5,0,A::pagePrevious),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,30,16,A::pageNext),39u);
    EXPECT_EQ(editorLibraryNavigation(40,16,30,16,A::first),0u);
    EXPECT_EQ(editorLibraryNavigation(40,16,0,0,A::last),39u);
    EXPECT_EQ(editorLibraryNavigation(3,16,12,99,A::next),2u);
}

TEST(EditorLibraryScroll, RevealsOnlyTheNeededRowsAndClampsToAFullLastViewport)
{
    EXPECT_EQ(editorLibraryScrollOffset(0,16,8),0);
    EXPECT_EQ(editorLibraryScrollOffset(3,16,8),0);
    EXPECT_EQ(editorLibraryScrollOffset(40,16,99),24);
    EXPECT_EQ(editorLibraryScrollOffset(40,16,-2),0);
    EXPECT_EQ(editorLibraryScrollOffset(40,16,2.25),2.25);
    EXPECT_EQ(editorLibraryReveal(40,16,0,15),0);
    EXPECT_EQ(editorLibraryReveal(40,16,0,16),1);
    EXPECT_EQ(editorLibraryReveal(40,16,8,7),7);
    EXPECT_EQ(editorLibraryReveal(40,16,0,39),24);
    EXPECT_EQ(editorLibraryReveal(40,16,8.5,8),8);
    EXPECT_EQ(editorLibraryReveal(40,16,8.5,24),9);
}

TEST(EditorResize, ParentDimensionsScaleTheWholeCanvasForDpiAndKeyboard)
{
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,1050,1,false),1.25f);
    EXPECT_FLOAT_EQ(*editorResizeScale(3200,2100,2,false),1.25f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,1170,1,true),1.25f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,840,1,false),1.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1280,1050,1,false),1.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1024,840,1,false),.8f);
    // Even a one-pixel height reduction must fit instead of retaining a scale
    // from the previous window size. Width and height are equally authoritative.
    EXPECT_FLOAT_EQ(*editorResizeScale(1600,1049,1,false),1049.f/840.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(1500,984,1,false),984.f/840.f);
    EXPECT_FLOAT_EQ(*editorResizeScale(100,100,1,false),.5f);
    EXPECT_FLOAT_EQ(*editorResizeScale(9000,9000,1,false),4.f);
    EXPECT_FALSE(editorResizeScale(0,840,1,false));
    EXPECT_FALSE(editorResizeScale(1280,0,1,false));
    EXPECT_FALSE(editorResizeScale(1280,840,0,false));
}

TEST(EditorResize, ParentOwnedBoundsKeepTheEffectsAndFooterVisible)
{
    // A wide tiled window must fit its height, rather than request a taller
    // parent that the window manager can refuse (issue #21).
    for (const bool keyboard : {false,true}) {
        const float logicalHeight=keyboard ? 936.f : 840.f;
        for (const float dpi : {1.f,1.5f,2.f}) {
            for (const auto bounds : {std::pair{1890,990},std::pair{900,1000},
                                      std::pair{1600,840},std::pair{1280,1050}}) {
                const int width=static_cast<int>(bounds.first*dpi);
                const int height=static_cast<int>(bounds.second*dpi);
                const float scale=*editorResizeScale(width,height,dpi,keyboard);
                EXPECT_LE(1280.f*scale*dpi,width+.001f);
                EXPECT_LE(logicalHeight*scale*dpi,height+.001f);
            }
        }
    }
}

TEST(EditorDisplayScale, AcceptsPercentagesWithoutDependingOnLocale)
{
    EXPECT_FLOAT_EQ(*editorScaleFromText("50"),.5f);
    EXPECT_FLOAT_EQ(*editorScaleFromText("400%"),4.f);
    EXPECT_FLOAT_EQ(*editorScaleFromText(" 125.5 % "),1.255f);
    EXPECT_EQ(editorScaleText(1.f),"100%");
    EXPECT_EQ(editorScaleText(1.255f),"125.5%");
    EXPECT_EQ(editorScaleText(990.f/840.f),"117.9%");
    for(const auto* text:{"","%","49.9","400.1","0","-100","nan","inf",
                          "1e100","125,5","100%%","100%garbage","100 200"})
        EXPECT_FALSE(editorScaleFromText(text))<<text;
    for(const int percent:editorScaleChoices)
        EXPECT_FLOAT_EQ(*editorScaleFromText(std::to_string(percent)+"%"),percent/100.f);
}

TEST(EditorDisplayScale, ActualScaleCanDifferFromTheRequestedPercentage)
{
    // A host/WM may keep a 1280x840 parent after the user requests 150%.
    const auto requested=*editorScaleFromText("150%");
    const auto accepted=*editorResizeScale(1280,840,1,false);
    EXPECT_GT(requested,accepted);
    EXPECT_EQ(editorScaleText(accepted),"100%");
    EXPECT_EQ(editorScaleText(*editorResizeScale(2560,1680,2,false)),"100%");
    EXPECT_EQ(editorScaleText(*editorResizeScale(1280,936,1,true)),"100%");
}

TEST(EditorDisplayScale, RepeatedKeyboardRequestsRetainTheChosenScaleAfterPixelRounding)
{
    for(const float dpi:{1.f,1.5f,2.f}) {
        float scale=.84375f;
        for(int cycle=0;cycle<20;++cycle) {
            const bool keyboard=cycle%2;
            const float logicalHeight=keyboard ? 936.f : 840.f;
            const int width=static_cast<int>(static_cast<int>(1280.f*scale)*dpi);
            const int height=static_cast<int>(static_cast<int>(logicalHeight*scale)*dpi);
            scale=*editorResizeScale(width,height,dpi,keyboard,scale);
            EXPECT_FLOAT_EQ(scale,.84375f);
        }
    }
    // Actual parent changes still win, even when only one pixel was removed.
    EXPECT_LT(*editorResizeScale(1600,1049,1,false,1.25f),1.25f);
}

TEST(EditorInteraction, DelayNotesPreserveSourceWordsAndFractionalRecall)
{
    const auto choices=editorChoices(35);
    ASSERT_EQ(choices.size(),19u);
    EXPECT_EQ(editorSelectedSourceValue(choices,0),1);
    EXPECT_EQ(editorChoiceDisplay(false,3,choices),"1/32");
    EXPECT_EQ(editorChoiceDisplay(false,7,choices),"1/16 dotted");
    EXPECT_EQ(editorChoiceDisplay(false,12,choices),"1/4");
    EXPECT_EQ(editorChoiceDisplay(false,19,choices),"1/1");
    EXPECT_TRUE(parameterUsesIntegerSteps(35));
    ProcessorBridge processor;submitSource(processor,35,7.25);
    const auto saved=processor.saveState();
    ProcessorBridge restored;restored.restoreState(saved.data(),static_cast<int>(saved.size()));
    EXPECT_NEAR(sourceValue(restored,35),7.25,1e-4);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,35),choices),"Custom");
    EXPECT_EQ(restored.parameter(35).minimum,0);
    EXPECT_EQ(restored.parameter(35).maximum,127);
}

TEST(EditorResize, ExplicitScaleSurvivesTheOldAuParentUntilActualGeometryChanges)
{
    const EditorResizeSize oldParent{1280,840},chosen{1600,1050};
    // The request already resized the editor. The host has not acknowledged it.
    EXPECT_FALSE(editorObservedResize(oldParent,oldParent,chosen,chosen));
    EXPECT_FALSE(editorObservedResize(oldParent,oldParent,{640,420},{640,420}));
    const auto acknowledged=editorObservedResize(chosen,oldParent,chosen,chosen);
    ASSERT_TRUE(acknowledged);EXPECT_EQ(*acknowledged,chosen);
    EXPECT_FLOAT_EQ(*editorResizeScale(acknowledged->width,acknowledged->height,1,false,1.25f),1.25f);
    EXPECT_FALSE(editorObservedResize(chosen,chosen,chosen,chosen));
    // A later frame drag still wins, including a one-pixel height reduction.
    const auto dragged=editorObservedResize({1600,1049},chosen,chosen,chosen);
    ASSERT_TRUE(dragged);EXPECT_EQ(dragged->height,1049);
    EXPECT_LT(*editorResizeScale(dragged->width,dragged->height,1,false,1.25f),1.25f);
}

TEST(EditorResize, DirectAuViewResizeDoesNotBounceBackToTheUnchangedContainer)
{
    const EditorResizeSize parent{1280,840},hostView{900,600},fitted{900,590};
    const auto target=editorObservedResize(parent,parent,hostView,parent);
    ASSERT_TRUE(target);EXPECT_EQ(*target,hostView);
    // Fitting the child does not turn its unchanged container into a new event.
    EXPECT_FALSE(editorObservedResize(parent,parent,fitted,fitted));
    EXPECT_FALSE(editorObservedResize({0,0},parent,hostView,parent));
    const auto constrained=editorObservedResize({1200,800},parent,{1600,1050},{1600,1050});
    ASSERT_TRUE(constrained);EXPECT_EQ(constrained->width,1200);
}

TEST(EditorInteraction, MidiAssignmentsReachRenderingAndSurvivePatchAndProjectRecall)
{
    for(const int version : {100,113,200})
    {
        SCOPED_TRACE(version);
        ProcessorBridge processor;
        std::string error;
        if(version!=200)ASSERT_TRUE(processor.loadPatch("Routing\nver="+std::to_string(version)+"\n86,45057\n87,19\n88,53248\n89,44\n50,127\n51,20\n",error))<<error;
        else {const auto patch=processor.savePatch();ASSERT_TRUE(processor.loadPatch(patch,error))<<error;}
        submitSource(processor,86,45057);
        submitSource(processor,88,53248);
        submitSource(processor,50,96.25);
        ASSERT_TRUE(processor.setMidiDestination(0,19));
        ASSERT_TRUE(processor.setMidiDestination(1,44));
        processor.prepare(48000,64);
        std::array<float,64> left{},right{};
        ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
        EXPECT_EQ(processor.engineForInspection().getRenderHistory().controllerRoutes[0].destination,19);
        ASSERT_TRUE(processor.setMidiDestination(0,-1));
        ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
        const auto& routes=processor.engineForInspection().getRenderHistory().controllerRoutes;
        EXPECT_EQ(routes[0].destination,-1);
        EXPECT_EQ(routes[0].source,45057);
        EXPECT_EQ(routes[1].destination,44);
        EXPECT_EQ(routes[1].source,53248);
        EXPECT_NEAR(sourceValue(processor,50),96.25,1e-4);
        // Host automation keeps its historical range; None is preserved in sound state.
        EXPECT_EQ(processor.parameter(87).minimum,0);
        EXPECT_EQ(processor.parameter(87).maximum,127);
        const auto state=processor.saveState();
        ProcessorBridge project;
        ASSERT_TRUE(project.tryRestoreState(state.data(),static_cast<int>(state.size())));
        EXPECT_EQ(project.midiDestination(0),-1);
        EXPECT_EQ(project.midiDestination(1),44);
        const auto patch=processor.savePatch();
        EXPECT_EQ(synth2::parseContinuousPatch(patch).values[87],-1);
        ProcessorBridge file;
        ASSERT_TRUE(file.loadPatch(patch,error))<<error;
        EXPECT_EQ(file.midiDestination(0),-1);
        EXPECT_EQ(file.midiDestination(1),44);
        file.prepare(48000,64);
        ASSERT_TRUE(file.process(left.data(),right.data(),64,120));
        EXPECT_EQ(file.engineForInspection().getRenderHistory().controllerRoutes[0].destination,-1);
        ASSERT_TRUE(file.setMidiDestination(0,19));
        ASSERT_TRUE(file.process(left.data(),right.data(),64,120));
        EXPECT_EQ(file.engineForInspection().getRenderHistory().controllerRoutes[0].destination,19);
        const auto before=file.saveState();
        EXPECT_FALSE(file.setMidiDestination(0,0));
        EXPECT_FALSE(file.setMidiDestination(0,-2));
        EXPECT_FALSE(file.setMidiDestination(2,19));
        EXPECT_EQ(file.saveState(),before);
    }
}

TEST(EditorInteraction, ImportedDisabledDestinationHasANameAndExportsWithoutBecomingZero)
{
    ProcessorBridge processor;
    std::string error;
    ASSERT_TRUE(processor.loadPatch("Disabled\nver=113\n86,45057\n87,-1\n88,57344\n89,-1\n",error))<<error;
    const auto before=processor.saveState();
    for(int route=0;route<2;++route)
    {
        EXPECT_EQ(processor.midiDestination(route),-1);
        EXPECT_EQ(editorChoiceDisplay(false,processor.midiDestination(route),editorChoices(87+route*2)),"None");
    }
    EXPECT_EQ(processor.saveState(),before); // Displaying the editor does not edit the patch.
    const auto exported=synth2::parseContinuousPatch(processor.savePatch());
    EXPECT_EQ(exported.values[87],-1);
    EXPECT_EQ(exported.values[89],-1);
}

TEST(EditorInteraction, MidiSensitivityShowsSignedDepthAndKeepsItsNeutralBandAndFractionalEdits)
{
    for(const int index : {50,51})
    {
        EXPECT_EQ(editorValueDisplay(index,0),"-100%");
        EXPECT_EQ(editorValueDisplay(index,20),"-68%");
        EXPECT_EQ(editorValueDisplay(index,63),"0%");
        EXPECT_EQ(editorValueDisplay(index,63.5),"0%");
        EXPECT_EQ(editorValueDisplay(index,64),"0%");
        EXPECT_EQ(editorValueDisplay(index,74),"+16%");
        EXPECT_EQ(editorValueDisplay(index,127),"+100%");
        EXPECT_EQ(editorValueDisplay(index,64.5),"+1%");
    }
}

TEST(EditorInteraction, BendRangeMenuKeepsHostRangeAndRecalledOutOfMenuValues)
{
    const auto choices=editorChoices(40);
    ASSERT_EQ(choices.size(),25u);
    EXPECT_EQ(choices.front().value,0);
    EXPECT_EQ(choices.back().value,24);
    ProcessorBridge processor;
    EXPECT_EQ(processor.parameter(40).minimum,0);
    EXPECT_EQ(processor.parameter(40).maximum,127);
    submitSource(processor,40,30);
    const auto state=processor.saveState();
    ProcessorBridge restored;
    ASSERT_TRUE(restored.tryRestoreState(state.data(),static_cast<int>(state.size())));
    EXPECT_NEAR(sourceValue(restored,40),30,1e-4);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,40),choices),"Custom");
    EXPECT_EQ(restored.saveState(),state);
    submitSource(restored,40,12);
    EXPECT_EQ(editorChoiceDisplay(false,sourceValue(restored,40),choices),"12 st");
}

TEST(EditorLibraryInteraction, VisibleTextTargetsSeparateFilteringFromSoundLoading)
{
    using A=LibraryRowAction;using C=EditorLibraryColumns;
    const auto tags=editorLibraryTags({"Warm","Velvet"},{"Mono"},1240,[](std::string_view text) {
        return static_cast<float>(text.size()*6);
    });
    ASSERT_EQ(tags.size(),3u);
    EXPECT_FALSE(tags[0].automatic);EXPECT_TRUE(tags[2].automatic);
    EXPECT_EQ(editorLibraryTarget(16,1240,tags).action,A::favorite);
    EXPECT_EQ(editorLibraryTarget(C::nameLeft+5,1240,tags).action,A::load);
    EXPECT_EQ(editorLibraryTarget(C::nameRight+1,1240,tags).action,A::none);
    EXPECT_EQ(editorLibraryTarget(C::bankLeft+1,1240,tags).action,A::bank);
    EXPECT_EQ(editorLibraryTarget(C::bankRight-1,1240,tags).action,A::bank);
    EXPECT_EQ(editorLibraryTarget(C::bankRight+1,1240,tags).action,A::none);
    for(const auto& tag:tags)
    {
        const auto hit=editorLibraryTarget(tag.left+tag.width/2,1240,tags);
        EXPECT_EQ(hit.action,A::tag);EXPECT_EQ(hit.tag,tag.text);
        EXPECT_EQ(editorLibraryTarget(tag.left+tag.width+1,1240,tags).action,A::none);
    }
    EXPECT_EQ(editorLibraryTarget(1200,1240,tags).action,A::none);
    EXPECT_EQ(editorLibraryTarget(1230,1240,tags).action,A::none);
    EXPECT_EQ(editorLibraryTarget(-1,1240,tags).action,A::none);
}

TEST(EditorLibraryInteraction, OverflowAndUtf8TagLabelsDoNotCreateHiddenClickTargets)
{
    using A=LibraryRowAction;
    const auto tags=editorLibraryTags({"柔らかい","Long label"},{"Hidden"},800,[](std::string_view text) {
        return text=="柔らかい" ? 40.f : text=="..." ? 18.f : 90.f;
    });
    ASSERT_EQ(tags.size(),2u);
    EXPECT_EQ(tags[0].text,"柔らかい");EXPECT_FALSE(tags[0].overflow);
    EXPECT_TRUE(tags[1].overflow);
    EXPECT_EQ(editorLibraryTarget(tags[0].left+1,800,tags).tag,"柔らかい");
    EXPECT_EQ(editorLibraryTarget(tags[1].left+1,800,tags).action,A::none);
    EXPECT_TRUE(editorLibraryTags({"Hidden"},{},700,[](std::string_view){return 40.f;}).empty());
}

TEST(EditorLibraryInteraction, FractionalScrollUsesTheRenderedRowAndRejectsClippedSpace)
{
    EXPECT_EQ(editorLibraryRow(0,510,.5,30,30),0u);
    EXPECT_EQ(editorLibraryRow(14,510,.5,30,30),0u);
    EXPECT_EQ(editorLibraryRow(15,510,.5,30,30),1u);
    EXPECT_EQ(editorLibraryRow(509,510,.5,30,30),17u);
    EXPECT_FALSE(editorLibraryRow(-1,510,.5,30,30));
    EXPECT_FALSE(editorLibraryRow(510,510,.5,30,30));
    EXPECT_FALSE(editorLibraryRow(0,510,0,0,30));
    EXPECT_FALSE(editorLibraryRow(60,510,0,2,30));
    EXPECT_EQ(editorLibraryRow(0,510,1.75,2,30),1u);
    EXPECT_FALSE(editorLibraryRow(8,510,1.75,2,30));
}

TEST(EditorLibraryInteraction, TagMenuAndInlineLinksShareCaseInsensitiveTogglingAndLimits)
{
    std::vector<std::string> tags{"Pad","柔らかい"};
    EXPECT_TRUE(editorToggleLibraryTag(tags,"pAD"));
    EXPECT_EQ(tags,(std::vector<std::string>{"柔らかい"}));
    EXPECT_TRUE(editorToggleLibraryTag(tags,"Mono"));
    EXPECT_EQ(tags,(std::vector<std::string>{"柔らかい","Mono"}));
    EXPECT_FALSE(editorToggleLibraryTag(tags,""));
    tags.clear();for(int i=0;i<32;++i)tags.push_back("Tag "+std::to_string(i));
    const auto full=tags;
    EXPECT_FALSE(editorToggleLibraryTag(tags,"More"));EXPECT_EQ(tags,full);
    EXPECT_TRUE(editorToggleLibraryTag(tags,"TAG 31"));
    EXPECT_TRUE(editorToggleLibraryTag(tags,"More"));EXPECT_EQ(tags.size(),32u);
}

TEST(EditorLibraryFilters, OverflowRetainsEveryHiddenFilterAndGapsHaveNoAction)
{
    using K=LibraryFilterKind;
    std::vector<EditorLibraryFilter> filters{{K::bank,"exact/bank/id","Bank: A very long bank path"},
        {K::query,"pad","Search: pad"},{K::favorites,{},"Favorites"},{K::tag,"柔らかい","Tag: 柔らかい"}};
    const auto measure=[](const auto& text){return static_cast<float>(text.size()*8);};
    const auto chips=editorLibraryFilterChips(filters,440,measure);
    ASSERT_EQ(chips.size(),2u);
    EXPECT_EQ(chips.front().index,0u);EXPECT_EQ(chips.front().remaining,0u);
    EXPECT_EQ(chips.front().width,240.f);
    EXPECT_EQ(chips.back().index,1u);EXPECT_EQ(chips.back().remaining,3u);
    EXPECT_LE(chips.back().left+chips.back().width,440.f);
    EXPECT_EQ(editorLibraryFilterChipAt(1,chips),&chips[0]);
    EXPECT_FALSE(editorLibraryFilterChipAt(242,chips));
    EXPECT_EQ(editorLibraryFilterChipAt(chips.back().left+1,chips),&chips.back());
    EXPECT_FALSE(editorLibraryFilterChipAt(439,chips));
    EXPECT_FALSE(editorLibraryFilterChipAt(-1,chips));
    EXPECT_EQ(filters[chips.back().index].value,"pad");
    EXPECT_EQ(filters.back().value,"柔らかい");
}

TEST(EditorLibraryFilters, FullWidthShowsAllFiltersAndNarrowWidthsStillExposeOverflow)
{
    using K=LibraryFilterKind;
    std::vector<EditorLibraryFilter> filters;
    for(int i=0;i<32;++i)filters.push_back({K::tag,"Tag "+std::to_string(i),"Tag "+std::to_string(i)});
    const auto measure=[](const auto& text){return static_cast<float>(text.size()*7);};
    const auto all=editorLibraryFilterChips(filters,3000,measure);
    ASSERT_EQ(all.size(),32u);EXPECT_EQ(all.back().remaining,0u);EXPECT_EQ(all.back().index,31u);
    const auto narrow=editorLibraryFilterChips(filters,40,measure);
    ASSERT_EQ(narrow.size(),1u);EXPECT_EQ(narrow.front().index,0u);
    EXPECT_EQ(narrow.front().remaining,32u);EXPECT_EQ(narrow.front().width,40.f);
    EXPECT_TRUE(editorLibraryFilterChips(filters,0,measure).empty());
    EXPECT_TRUE(editorLibraryFilterChips({},400,measure).empty());
    const std::vector<EditorLibraryFilter> shortLabels{{K::tag,"a","a"},{K::tag,"b","b"}};
    const auto exact=editorLibraryFilterChips(shortLabels,80,measure);
    ASSERT_EQ(exact.size(),2u);EXPECT_EQ(exact.back().remaining,0u);
}
