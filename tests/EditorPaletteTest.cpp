#include "editor/EditorPalette.h"
#include "host/EditorPaletteStore.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>

using namespace synth2::editor;
using synth2::host::EditorPaletteStore;
namespace {
class EditorPaletteTest : public ::testing::Test
{
protected:
    std::filesystem::path directory=std::filesystem::temp_directory_path()/
        ("synth2-palette-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    void SetUp() override {std::filesystem::create_directories(directory);}
    void TearDown() override {std::error_code ignored;std::filesystem::remove_all(directory,ignored);}
    void write(std::string_view text) {std::ofstream out(directory/"palette.xml");out<<text;}
};
}
TEST(EditorColorTest,HexHasExplicitRgbaOrderAndStrictValidation)
{
    const EditorColor expected{0x12,0xab,0x34,0x80};
    ASSERT_TRUE(EditorColor::parse("#12AB3480"));
    EXPECT_EQ(*EditorColor::parse("#12AB3480"),expected);
    EXPECT_EQ(expected.hex(),"#12ab3480");
    EXPECT_EQ(EditorColor::parse("#abcdef")->alpha,255);
    EXPECT_EQ(EditorColor::parse("#00000000")->alpha,0);
    for(const auto* invalid:{"abcdef","#abc","#123456789","#12345g"," #123456","#123456\n"})
        EXPECT_FALSE(EditorColor::parse(invalid))<<invalid;
}
TEST(EditorPaletteModel,GroupsInheritAndOnlyExplicitOverridesStayIndependent)
{
    EditorPalette palette;const EditorColor red{255,0,0,128},blue{0,0,255,64};
    ASSERT_TRUE(palette.set(PaletteRole::panel,red));
    EXPECT_EQ(palette.resolve(PaletteRole::panel,paletteGroup("OSCILLATORS")),red);
    EXPECT_NE(palette.resolve(PaletteRole::panel,paletteGroup("FILTER")),red);
    ASSERT_TRUE(palette.set(PaletteRole::panel,blue,paletteGroup("OSCILLATORS")));
    EXPECT_EQ(palette.resolve(PaletteRole::panel,paletteGroup("OSCILLATORS")),blue);
    EXPECT_EQ(palette.resolve(PaletteRole::panel,paletteGroup("AMPLIFIER")),red);
    palette.inherit(paletteGroup("FILTER"));
    EXPECT_EQ(palette.resolve(PaletteRole::panel,paletteGroup("FILTER")),red);
    EXPECT_FALSE(palette.set(PaletteRole::header,blue,paletteGroup("FILTER")));
    EXPECT_FALSE(palette.set(PaletteRole::text,blue,100));
    EXPECT_FALSE(palette.set(PaletteRole::count,blue));
}
TEST(EditorPaletteModel,OpaqueCanvasAndIndependentInstances)
{
    EditorPalette a,b;const EditorColor invisible{12,34,56,0};
    a.set(PaletteRole::background,invisible);a.set(PaletteRole::text,invisible);
    EXPECT_EQ(a.resolve(PaletteRole::background).alpha,255);
    EXPECT_EQ(a.resolve(PaletteRole::text),invisible);
    EXPECT_EQ(b,EditorPalette{});
}
TEST(EditorPaletteModel,CancelRestoresOverridesAndApplyAcceptsPreview)
{
    EditorPalette palette;const auto original=palette;
    EditorPaletteEdit edit;edit.begin(palette);
    palette.inherit(paletteGroup("FILTER"));palette.set(PaletteRole::text,{0,0,0,0});
    edit.begin(palette); // Reopening must retain the first cancellation baseline.
    edit.cancel(palette);EXPECT_EQ(palette,original);EXPECT_FALSE(edit.active);
    edit.begin(palette);palette.set(PaletteRole::accent,{1,2,3,4});edit.apply();
    const auto applied=palette;edit.cancel(palette);EXPECT_EQ(palette,applied);
}
TEST(EditorPaletteModel,UnsavedChangesTrackValuesAlphaAndInheritanceInsteadOfEditHistory)
{
    EditorPalette palette;EditorPaletteEdit edit;edit.begin(palette);
    EXPECT_FALSE(edit.modified(palette));
    const auto original=palette;
    palette.set(PaletteRole::accent,palette.resolve(PaletteRole::accent));
    EXPECT_FALSE(edit.modified(palette));
    auto transparent=palette.resolve(PaletteRole::accent);transparent.alpha=0;
    palette.set(PaletteRole::accent,transparent);EXPECT_TRUE(edit.modified(palette));
    palette=original;EXPECT_FALSE(edit.modified(palette));
    // An explicit equal-color override still changes inheritance behavior.
    palette.set(PaletteRole::text,palette.resolve(PaletteRole::text),paletteGroup("OSCILLATORS"));
    EXPECT_TRUE(edit.modified(palette));
    palette.inherit(paletteGroup("OSCILLATORS"));EXPECT_FALSE(edit.modified(palette));
    palette.inherit(paletteGroup("FILTER"));EXPECT_TRUE(edit.modified(palette));
    edit.cancel(palette);EXPECT_FALSE(edit.modified(palette));EXPECT_EQ(palette,original);
    edit.begin(palette);palette.set(PaletteRole::accent,transparent);edit.apply();
    EXPECT_FALSE(edit.modified(palette));
}
TEST_F(EditorPaletteTest,RoundTripPreservesTransparencyAndClearedDefaultOverrides)
{
    EditorPalette palette;palette.inherit(paletteGroup("FILTER"));
    palette.set(PaletteRole::panel,{1,2,3,4});
    palette.set(PaletteRole::text,{255,128,0,0},paletteGroup("DELAY"));
    std::string error;ASSERT_TRUE(EditorPaletteStore::save(directory/"palette.xml",palette,error))<<error;
    EditorPalette loaded;ASSERT_TRUE(EditorPaletteStore::load(directory/"palette.xml",loaded,error))<<error;
    EXPECT_EQ(loaded,palette);
    EXPECT_FALSE(loaded.groups[paletteGroup("FILTER")][static_cast<std::size_t>(PaletteRole::panel)]);
    // Replacing an existing preference file must also succeed.
    ASSERT_TRUE(EditorPaletteStore::save(directory/"palette.xml",EditorPalette{},error))<<error;
    ASSERT_TRUE(EditorPaletteStore::load(directory/"palette.xml",loaded,error))<<error;
    EXPECT_EQ(loaded,EditorPalette{});
}
TEST_F(EditorPaletteTest,MalformedPreferencesDoNotPartiallyChangeThePalette)
{
    EditorPalette palette;palette.set(PaletteRole::accent,{1,2,3,4});const auto before=palette;
    for(const auto* invalid:{
        "<Synth2Palette version='2'/>",
        "<Synth2Palette version='1'><Color role='text' rgba='#123456'/><Color role='bad' rgba='#abcdef'/></Synth2Palette>",
        "<Synth2Palette version='1'><Color role='text' rgba='#123456'/><Color role='text' rgba='#abcdef'/></Synth2Palette>",
        "<Synth2Palette version='1'><Color role='text' group='missing' rgba='#123456'/></Synth2Palette>",
        "<Synth2Palette version='1'><Color role='background' rgba='#12345600'/></Synth2Palette>",
        "<Synth2Palette version='1'><Color role='header' group='filter' rgba='#123456'/></Synth2Palette>",
        "<Synth2Palette"}) {
        write(invalid);std::string error;
        EXPECT_FALSE(EditorPaletteStore::load(directory/"palette.xml",palette,error));EXPECT_FALSE(error.empty());EXPECT_EQ(palette,before);
    }
    write(std::string(65537,'x'));std::string error;
    EXPECT_FALSE(EditorPaletteStore::load(directory/"palette.xml",palette,error));EXPECT_EQ(palette,before);
}
TEST_F(EditorPaletteTest,MissingPreferencesPreserveDefaultsAndWriteFailureIsReported)
{
    EditorPalette palette;std::string error;
    EXPECT_TRUE(EditorPaletteStore::load(directory/"missing.xml",palette,error));EXPECT_EQ(palette,EditorPalette{});
    std::ofstream(directory/"file")<<"occupied";
    EXPECT_FALSE(EditorPaletteStore::save(directory/"file"/"palette.xml",palette,error));EXPECT_FALSE(error.empty());
    EXPECT_EQ(palette,EditorPalette{});
}

TEST_F(EditorPaletteTest,DeletingPreferencesRestoresDefaultsWhenTheSameEditorReloads)
{
    const auto path=directory/"palette.xml";
    EditorPalette palette;palette.set(PaletteRole::accent,{1,2,3,128});
    palette.inherit(paletteGroup("FILTER"));const auto custom=palette;
    std::string error;ASSERT_TRUE(EditorPaletteStore::save(path,palette,error))<<error;
    palette=EditorPalette{};
    ASSERT_TRUE(EditorPaletteStore::load(path,palette,error))<<error;EXPECT_EQ(palette,custom);
    ASSERT_TRUE(std::filesystem::remove(path));
    ASSERT_TRUE(EditorPaletteStore::load(path,palette,error))<<error;
    EXPECT_EQ(palette,EditorPalette{});
}
