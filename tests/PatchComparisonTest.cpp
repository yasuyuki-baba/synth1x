#include "host/PatchComparison.h"
#include <gtest/gtest.h>
#include <algorithm>
using namespace synth1x::host;
namespace
{
void load(ProcessorBridge& p,PresetBrowserState& b,const std::string& name,int cutoff)
{
    std::string error;
    ASSERT_TRUE(p.loadProgram(name+"\nver=113\n19,"+std::to_string(cutoff)+"\n",7,error,name))<<error;
    b.selectedId="bank/"+name;b.patchName=name;b.capture(p);
}
uint32_t word(const std::vector<uint8_t>& bytes,std::size_t offset)
{
    uint32_t result=0;for(int i=0;i<4;++i)result|=uint32_t(bytes.at(offset+i))<<(8*i);return result;
}
std::size_t comparisonOffset(const std::vector<uint8_t>& bytes)
{
    const auto end=12+word(bytes,8);return end+4+word(bytes,end);
}
}
TEST(PatchComparison, BrowsingAndEditingBothSlotsPreservesExactSourceAndSharedFilters)
{
    ProcessorBridge p;PresetBrowserState view;load(p,view,"基準 Pad",100);
    view.query="pad";view.bankId="Bank.zip/Pads";view.selectedTags={"Warm"};view.favoritesOnly=true;view.page=2;view.scroll=3;
    PatchComparison comparison;comparison.reset(p,view);
    p.setNormalizedValue(19,.4321);ASSERT_TRUE(p.setMidiDestination(0,-1));
    const auto a=p.saveState();const auto baselineA=view.baseline;
    ASSERT_TRUE(comparison.select(1,p,view));
    EXPECT_EQ(comparison.other().state,a);
    load(p,view,"Candidate lead",37);p.setNormalizedValue(3,.314159);
    const auto b=p.saveState();
    ASSERT_TRUE(comparison.select(0,p,view));
    EXPECT_EQ(p.saveState(),a);EXPECT_EQ(p.midiDestination(0),-1);
    EXPECT_EQ(view.patchName,"基準 Pad");EXPECT_EQ(view.selectedId,"bank/基準 Pad");
    EXPECT_EQ(view.baseline,baselineA);EXPECT_TRUE(view.modified(p));
    EXPECT_EQ(view.query,"pad");EXPECT_EQ(view.bankId,"Bank.zip/Pads");EXPECT_TRUE(view.favoritesOnly);
    EXPECT_EQ(view.selectedTags,(std::vector<std::string>{"Warm"}));EXPECT_EQ(view.page,2);EXPECT_EQ(view.scroll,3);
    ASSERT_TRUE(comparison.select(1,p,view));EXPECT_EQ(p.saveState(),b);EXPECT_TRUE(view.modified(p));
    EXPECT_EQ(view.patchName,"Candidate lead");
    p.setOversamplingFactor(8);
    ASSERT_TRUE(comparison.select(0,p,view));EXPECT_EQ(p.oversamplingFactor(),8);
    ASSERT_TRUE(comparison.select(1,p,view));EXPECT_EQ(p.oversamplingFactor(),8);
}
TEST(PatchComparison, CopiesWorkInBothDirectionsWithoutChangingTheSelectedSide)
{
    ProcessorBridge p;PresetBrowserState view;load(p,view,"A",120);
    PatchComparison comparison;comparison.reset(p,view);
    ASSERT_TRUE(comparison.select(1,p,view));load(p,view,"B",10);
    const auto b=p.saveState();
    ASSERT_TRUE(comparison.copyFrom(1,p,view));EXPECT_EQ(p.saveState(),b);EXPECT_EQ(comparison.activeSlot(),1);
    ASSERT_TRUE(comparison.select(0,p,view));EXPECT_EQ(p.saveState(),b);EXPECT_EQ(view.patchName,"B");
    p.setNormalizedValue(19,.125);const auto edit=p.saveState();
    ASSERT_TRUE(comparison.select(1,p,view));
    ASSERT_TRUE(comparison.copyFrom(0,p,view));EXPECT_EQ(p.saveState(),edit);EXPECT_EQ(comparison.activeSlot(),1);
    EXPECT_TRUE(view.modified(p));EXPECT_EQ(comparison.other().identity.baseline,view.baseline);
    EXPECT_FALSE(comparison.select(2,p,view));EXPECT_FALSE(comparison.copyFrom(-1,p,view));EXPECT_EQ(p.saveState(),edit);
}
TEST(PatchComparison, SessionRecallRetainsBothSoundsBaselinesAndSelectionWithoutFiles)
{
    ProcessorBridge p;PresetBrowserState view;load(p,view,"Reference",112);
    PatchComparison comparison;comparison.reset(p,view);
    p.setNormalizedValue(19,.371);const auto a=p.saveState();
    ASSERT_TRUE(comparison.select(1,p,view));load(p,view,"Candidate",21);
    p.setNormalizedValue(3,.401);const auto b=p.saveState();view.query="common search";
    const auto saved=savePresetSession(p,view,&comparison);
    ProcessorBridge recalled;PresetBrowserState browser;PatchComparison restored;
    ASSERT_EQ(restorePresetSession(saved.data(),saved.size(),recalled,browser,&restored),saved.size());
    EXPECT_EQ(restored.activeSlot(),1);EXPECT_EQ(recalled.saveState(),b);EXPECT_TRUE(browser.modified(recalled));
    EXPECT_EQ(restored.other().state,a);
    EXPECT_EQ(savePresetSession(recalled,browser,&restored),saved);
    ASSERT_TRUE(restored.select(0,recalled,browser));EXPECT_EQ(recalled.saveState(),a);
    EXPECT_EQ(browser.query,"common search");EXPECT_TRUE(browser.modified(recalled));EXPECT_EQ(browser.patchName,"Reference");
    // Older sessions initialize both sides from their single sounding patch.
    const auto legacy=savePresetSession(p,view);
    ASSERT_EQ(restorePresetSession(legacy.data(),legacy.size(),recalled,browser,&restored),legacy.size());
    EXPECT_EQ(restored.activeSlot(),0);EXPECT_EQ(restored.other().state,b);
    ASSERT_TRUE(restored.select(1,recalled,browser));EXPECT_EQ(recalled.saveState(),b);
}
TEST(PatchComparison, InvalidInactivePayloadAndSelectionRejectTheWholeRecall)
{
    ProcessorBridge p;PresetBrowserState view;load(p,view,"Reference",100);
    PatchComparison comparison;comparison.reset(p,view);ASSERT_TRUE(comparison.select(1,p,view));load(p,view,"Candidate",20);
    const auto saved=savePresetSession(p,view,&comparison);const auto offset=comparisonOffset(saved);
    const auto before=p.saveState(),other=comparison.other().state;const auto browser=view.serialize();
    auto truncated=saved;truncated.pop_back();
    auto badSound=saved;badSound.at(offset+8+12)=0;
    auto badView=saved;
    const std::string document(saved.begin(),saved.end());
    const auto baseline=document.find("<baseline>",offset+8);ASSERT_NE(baseline,std::string::npos);
    badView.at(baseline+10)='x';
    auto badSlot=saved;badSlot.at(offset)=2;
    auto nested=saved;nested.at(offset+8+4)=3;
    for(const auto& bad:{truncated,badSound,badView,badSlot,nested}) {
        EXPECT_EQ(restorePresetSession(bad.data(),bad.size(),p,view,&comparison),-1);
        EXPECT_EQ(p.saveState(),before);EXPECT_EQ(view.serialize(),browser);
        EXPECT_EQ(comparison.activeSlot(),1);EXPECT_EQ(comparison.other().state,other);
        EXPECT_EQ(restorePresetSession(bad.data(),bad.size(),p,view),-1);EXPECT_EQ(p.saveState(),before);
    }
}
