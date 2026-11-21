#include "editor/EditorLibraryInteraction.h"
#include "../Synth1X.h"
#include "EditorControls.h"
#include "EditorStyle.h"
#include "IPopupMenuControl.h"
#include "EditorControlTags.h"
#include "editor/EditorPages.h"
#include "editor/EditorBankMenu.h"
#include "editor/EditorLibraryNavigation.h"
#include "editor/EditorLibraryScroll.h"
#include <cmath>
#include "editor/EditorTextFit.h"
#include "editor/EditorPresetDisplay.h"
#include <algorithm>
#include <set>
#include <map>
#include <stdexcept>
#include <chrono>
#include "../src/patch/PatchFormat.h"

using namespace iplug;
using namespace iplug::igraphics;
using namespace synth1x::editor::native;

namespace {
std::vector<synth1x::editor::EditorBankMenuRow> libraryBankRows(const std::vector<synth1x::host::CatalogBank>& banks)
{
    std::vector<std::pair<std::string,std::string>> entries;
    for(const auto& bank:banks)entries.emplace_back(bank.id,bank.name);
    return synth1x::editor::editorBankMenuRows(synth1x::editor::editorBankMenu(entries));
}
}

void Synth1X::refreshBrowser()
{
    const auto appearance=nativeAppearance(editorPalette);
    auto *graphics = GetUI();
    if (!graphics)
        return;
#ifdef _WIN32
    std::replace(libraryFilter.bankId.begin(), libraryFilter.bankId.end(), '\\', '/');
#endif
    const auto banks = catalog.banks();
    if (catalogLoaded && !libraryFilter.bankId.empty() &&
        std::none_of(catalog.patches().begin(), catalog.patches().end(),
                     [&](const auto &patch) { return patch.bankId == libraryFilter.bankId; }))
        libraryFilter.bankId.clear();
    const auto matches = catalog.search(libraryFilter);
    if(catalogLoaded)resultOffset=synth1x::editor::editorLibraryScrollOffset(matches.size(),visibleRows,resultOffset);
    const auto setText = [&](int tag, const std::string &value)
    {
        if (auto *c = graphics->GetControlWithTag(tag))
        {
            c->As<ITextControl>()->SetStr(value.c_str());
            c->SetDirty(false);
        }
    };
    const auto setButton = [&](int tag, const std::string &value)
    {
        if (auto *c = graphics->GetControlWithTag(tag))
        {
            c->As<IVButtonControl>()->SetLabelStr(value.c_str());
            c->SetDirty(false);
        }
    };
    const auto status=synth1x::editor::editorLibraryStatus({libraryJob.valid(),catalogLoaded,
        libraryFilter.registeredOnly,libraryOperation,catalog.patches().size(),matches.size(),visibleRows,resultOffset});
    setText(bankTag,status.summary);
    if(auto* control=graphics->GetControlWithTag(libraryActivityTag)) {
        if(libraryJob.valid() && !control->GetAnimationFunction())control->SetAnimation(DefaultAnimationFunc,1100);
        else if(!libraryJob.valid() && control->GetAnimationFunction()) {
            control->SetAnimation(nullptr);control->SetDirty(false);
        }
    }
    const auto selected = std::find_if(catalog.patches().begin(), catalog.patches().end(),
                                       [&](const auto &p) { return p.id == selectedPatchId; });
    const bool canEdit = selected != catalog.patches().end();
    const bool modified=browser.modified(processor);
    const auto display=synth1x::editor::editorPresetDisplay(currentPatch,
        canEdit ? selected->bankName : std::string{},modified,
        canEdit ? selected->tags : std::vector<std::string>{},
        canEdit ? selected->visibleAutomaticTags() : std::vector<std::string>{});
    if(auto* c=graphics->GetControlWithTag(presetTag))
        c->As<PresetDisplayControl>()->setDisplay(display);
    if(auto* control=graphics->GetControlWithTag(comparisonTag)) {
        control->SetValue(comparison.activeSlot());control->SetDirty(false);
        control->SetDisabled(libraryJob.valid());
        control->SetTooltip("Switch A/B. Each side keeps its sound and edits; Library loads affect the selected side.");
    }
    for(const auto tag:{copyATag,copyBTag})if(auto* control=graphics->GetControlWithTag(tag)) {
        control->SetDisabled(libraryJob.valid());
        control->SetTooltip(tag==copyATag ? "Replace B with A, including edits." : "Replace A with B, including edits.");
    }
    setText(statusTag,browserStatus);
    setText(librarySearchTag, libraryFilter.query);
    std::string bankLabel = "All banks";
    for (const auto &bank : banks)
        if (bank.id == libraryFilter.bankId)
        {
            bankLabel = bank.name;
            break;
        }
    const auto setFilterLabel=[&](int tag,const std::string& value,bool keepEnd=false) {
        if(auto* control=graphics->GetControlWithTag(tag)) {
            const auto measure=[&](std::string_view text) {
                IRECT bounds;const std::string copy(text);
                graphics->MeasureText(IText(Typography::button,appearance.text,"Synth1XUI"),copy.c_str(),bounds);return bounds.W();
            };
            setButton(tag,synth1x::editor::editorFitText(value,control->GetRECT().W()-24.f,measure,keepEnd));
        }
    };
    const auto bankRows=libraryBankRows(banks);
    for(const auto& [tag,direction]:{std::pair{previousBankTag,-1},std::pair{nextBankTag,1}})
        if(auto* control=graphics->GetControlWithTag(tag)) {
            control->SetDisabled(!synth1x::editor::editorAdjacentBank(bankRows,libraryFilter.bankId,direction));
            control->SetTooltip(direction<0 ? "Previous bank" : "Next bank");
        }
    setFilterLabel(bankFilterTag,bankLabel,true);
    if(auto* control=graphics->GetControlWithTag(bankFilterTag))control->SetTooltip(bankLabel.c_str());
    if(auto* control=graphics->GetControlWithTag(librarySearchTag))
        control->SetTooltip("Search names, banks and tags. Use several words to narrow results. Drop .zip, .rar, .7z or .sy1 files onto the sound list to add sounds.");
    const auto& selectedTags=libraryFilter.selectedTags;
    const auto tagLabel=selectedTags.empty() ? std::string("All tags") : selectedTags.size()==1 ? selectedTags.front() : "Tags: "+std::to_string(selectedTags.size())+" (all)";
    setFilterLabel(tagFilterTag,tagLabel);
    if(auto* control=graphics->GetControlWithTag(tagFilterTag))
        control->SetTooltip(("Match all selected tags: "+synth1x::host::PatchCatalog::tagText(selectedTags)).c_str());
    setButton(favoritesTag, libraryFilter.favoritesOnly ? "Favorites: on" : "Favorites: off");
    using Kind=synth1x::editor::LibraryFilterKind;
    std::vector<synth1x::editor::EditorLibraryFilter> activeFilters;
    if(!libraryFilter.bankId.empty())activeFilters.push_back({Kind::bank,libraryFilter.bankId,"Bank: "+bankLabel});
    if(!libraryFilter.query.empty())activeFilters.push_back({Kind::query,libraryFilter.query,"Search: "+libraryFilter.query});
    if(libraryFilter.favoritesOnly)activeFilters.push_back({Kind::favorites,{},"Favorites"});
    if(libraryFilter.registeredOnly)activeFilters.push_back({Kind::registration,{},"Registered sounds"});
    for(const auto& tag:selectedTags)activeFilters.push_back({Kind::tag,tag,"Tag: "+tag});
    if(auto* control=graphics->GetControlWithTag(libraryFiltersTag))
        control->As<LibraryFiltersControl>()->setFilters(std::move(activeFilters));
    if(auto* control=graphics->GetControlWithTag(libraryMatchesTag))
        control->SetTooltip(libraryJob.valid() ? status.emptyDetail.c_str() : "Matching sounds. Ctrl/Cmd+F: Search. Up/Down: Browse. Enter: Load. Click a filter to remove only that condition.");
    if (auto *c = graphics->GetControlWithTag(libraryListTag)) {
        c->As<CatalogListControl>()->setEmptyMessage(status.emptyTitle,status.emptyDetail);
        c->As<CatalogListControl>()->setResults(matches, resultOffset);
    }
    setText(selectedNameTag,"Manual tags");
    setText(tagsEditTag, canEdit ? synth1x::host::PatchCatalog::tagText(selected->tags) : "");
    const auto tagHelp=canEdit
        ? "Manual tags for " + selected->patch.name + ". Separate tags with commas; Enter saves. Muted tags in the list are automatic."
        : std::string("Select a sound to edit its tags.");
    for(const auto tag:{selectedNameTag,tagsEditTag})
        if(auto* control=graphics->GetControlWithTag(tag))control->SetTooltip(tagHelp.c_str());
    if (auto *c = graphics->GetControlWithTag(tagsEditTag))c->SetDisabled(!canEdit);
    if(auto* control=graphics->GetControlWithTag(4016))
        control->SetTooltip("Hide or restore automatic tags for the selected sound.");
    setText(libraryMatchesTag,status.matches);
}

bool Synth1X::handleLibraryKey(const IKeyPress& key,bool up)
{
    auto* graphics=GetUI();
    if(!graphics || graphics->GetControlInTextEntry())return false;
    if(auto* menu=graphics->GetPopupMenuControl();menu && menu->GetExpanded())return false;
    if(key.C && !key.A && !key.S && (key.VK=='F' || key.VK=='f')) {
        if(!up) {
            releaseComputerNotes();showPage(synth1x::editor::editorLibraryView);
            if(auto* search=graphics->GetControlWithTag(librarySearchTag)) {
                auto* control=search->As<SearchControl>();
                graphics->CreateTextEntry(*control,control->GetText(),control->GetRECT(),control->GetStr());
            }
        }
        return true;
    }
    if(page!=synth1x::editor::editorLibraryView || key.C || key.A || key.S)return false;
    using Action=synth1x::editor::LibraryNavigation;
    std::optional<Action> action;
    switch(key.VK) {
        case kVK_UP:action=Action::previous;break;
        case kVK_DOWN:action=Action::next;break;
        case kVK_PRIOR:action=Action::pagePrevious;break;
        case kVK_NEXT:action=Action::pageNext;break;
        case kVK_HOME:action=Action::first;break;
        case kVK_END:action=Action::last;break;
        case kVK_RETURN:case kVK_ESCAPE:break;
        default:return false;
    }
    if(up)return true;
    const auto matches=catalog.search(libraryFilter);
    if(matches.empty())return true;
    const auto locate=[&](const std::string& id)->std::optional<std::size_t> {
        for(std::size_t i=0;i<matches.size();++i)if(catalog.patches()[matches[i]].id==id)return i;
        return {};
    };
    if(key.VK==kVK_ESCAPE) {
        keyboardPatchId.clear();
        if(auto current=locate(selectedPatchId))resultOffset=synth1x::editor::editorLibraryReveal(matches.size(),visibleRows,resultOffset,*current);
        refreshBrowser();return true;
    }
    auto current=locate(keyboardPatchId);
    if(!current)current=locate(selectedPatchId);
    if(key.VK==kVK_RETURN) {
        const auto position=current.value_or(std::min(static_cast<std::size_t>(resultOffset),matches.size()-1));
        keyboardPatchId=catalog.patches()[matches[position]].id;
        loadLibraryPatch(matches[position]);return true;
    }
    const auto next=synth1x::editor::editorLibraryNavigation(matches.size(),visibleRows,current,static_cast<std::size_t>(resultOffset),*action);
    keyboardPatchId=catalog.patches()[matches[*next]].id;
    resultOffset=synth1x::editor::editorLibraryReveal(matches.size(),visibleRows,resultOffset,*next);refreshBrowser();return true;
}

void Synth1X::changeFilter(const char *query)
{
    libraryFilter.query = query;
    keyboardPatchId.clear();
    resultOffset = 0;
    browserStatus.clear();
    showPage(synth1x::editor::editorLibraryView);
    refreshBrowser();
}

void Synth1X::showPresetMenu(IControl *) { showPage(synth1x::editor::editorLibraryView); }

void Synth1X::selectBank(const std::string& id)
{
    libraryFilter.registeredOnly=false;
    libraryFilter.registeredIds.clear();
    libraryFilter.bankId=id;
    keyboardPatchId.clear();resultOffset=0;browserStatus.clear();refreshBrowser();
}

void Synth1X::stepBank(int direction)
{
    const auto next=synth1x::editor::editorAdjacentBank(libraryBankRows(catalog.banks()),libraryFilter.bankId,direction);
    if(next)selectBank(*next);
}

void Synth1X::showBankMenu(IControl *caller)
{
    presetMenu = std::make_unique<IPopupMenu>("Banks");
    const auto rows=libraryBankRows(catalog.banks());
    presetMenu->SetFunction([this,rows](IPopupMenu* menu) {
        const auto* item=menu->GetChosenItem();
        if(!item || item->GetTag()<0 || static_cast<std::size_t>(item->GetTag())>=rows.size())return;
        const auto& row=rows[item->GetTag()];
        if(!row.selectable)return;
        selectBank(row.bankId);
    });
    for(std::size_t i=0;i<rows.size();++i) {
        const auto& row=rows[i];
        const auto label=std::string(row.depth*3,' ')+row.label;
        const int flags=!row.selectable ? IPopupMenu::Item::kDisabled :
            libraryFilter.bankId==row.bankId ? IPopupMenu::Item::kChecked : 0;
        presetMenu->AddItem(new IPopupMenu::Item(label.c_str(),flags,static_cast<int>(i)));
    }
    showScrollableLibraryMenu(caller,caller->GetRECT());
}
void Synth1X::showScrollableLibraryMenu(IControl* caller,const IRECT& bounds)
{
    auto* graphics=caller->GetUI();
    // The pinned SDK's custom popup has no wheel handler. Use the platform
    // menu for long bank/tag lists; keep unrelated menus custom-rendered.
    auto* popup=graphics->GetPopupMenuControl();
    const bool restorePopup=popup!=nullptr;
    const auto popupText=popup ? popup->GetText() : DEFAULT_TEXT;
    graphics->RemovePopupMenuControl();
    graphics->CreatePopupMenu(*caller,*presetMenu,bounds);
    if(restorePopup)graphics->AttachPopupMenuControl(popupText);
}

void Synth1X::toggleTagFilter(const std::string& tag)
{
    if(!synth1x::editor::editorToggleLibraryTag(libraryFilter.selectedTags,tag)) {
        browserStatus="Select up to 32 tags.";refreshBrowser();return;
    }
    libraryFilter.tag.clear();keyboardPatchId.clear();
    resultOffset=0;browserStatus.clear();refreshBrowser();
}

void Synth1X::removeLibraryFilter(const synth1x::editor::EditorLibraryFilter& filter)
{
    using Kind=synth1x::editor::LibraryFilterKind;
    switch(filter.kind) {
        case Kind::query:libraryFilter.query.clear();break;
        case Kind::bank:libraryFilter.bankId.clear();break;
        case Kind::tag: {
            const auto key=synth1x::editor::editorLibraryTagKey(filter.value);
            std::erase_if(libraryFilter.selectedTags,[&](const auto& tag) {
                return synth1x::editor::editorLibraryTagKey(tag)==key;
            });
            libraryFilter.tag.clear();break;
        }
        case Kind::favorites:libraryFilter.favoritesOnly=false;break;
        case Kind::registration:
            libraryFilter.registeredOnly=false;libraryFilter.registeredIds.clear();break;
    }
    keyboardPatchId.clear();resultOffset=0;browserStatus.clear();refreshBrowser();
}

void Synth1X::showFilterMenu(IControl* caller,const std::vector<synth1x::editor::EditorLibraryFilter>& filters,const IRECT& bounds)
{
    presetMenu=std::make_unique<IPopupMenu>("Remove filter");
    for(const auto& filter:filters)presetMenu->AddItem(("Remove "+filter.label).c_str());
    presetMenu->SetFunction([this,filters](IPopupMenu* menu) {
        const int index=menu->GetChosenItemIdx();
        if(index>=0 && static_cast<std::size_t>(index)<filters.size())removeLibraryFilter(filters[index]);
    });
    showScrollableLibraryMenu(caller,bounds);
}

void Synth1X::showTagMenu(IControl *caller)
{
    const auto lower=synth1x::editor::editorLibraryTagKey;
    std::map<std::string,std::string> unique;
    auto scoped=libraryFilter;scoped.tag.clear();scoped.selectedTags.clear();scoped.query.clear();
    for(auto i:catalog.search(scoped))for(const auto& tag:catalog.patches()[i].allTags())unique.emplace(lower(tag),tag);
    for(const auto& tag:libraryFilter.selectedTags)unique.emplace(lower(tag),tag);
    std::vector<std::string> tags;for(const auto& [key,label]:unique)tags.push_back(label);
    const auto selected=[&](const auto& tag) {return std::any_of(libraryFilter.selectedTags.begin(),libraryFilter.selectedTags.end(),[&](const auto& other){return lower(tag)==lower(other);});};
    presetMenu=std::make_unique<IPopupMenu>("Tags (match all)");
    presetMenu->AddItem("All tags",-1,libraryFilter.selectedTags.empty() ? IPopupMenu::Item::kChecked : 0);
    for(const auto& tag:tags)presetMenu->AddItem(tag.c_str(),-1,selected(tag) ? IPopupMenu::Item::kChecked : 0);
    presetMenu->SetFunction([this,tags](IPopupMenu* menu) {
        const int index=menu->GetChosenItemIdx();
        if(index<0 || index>static_cast<int>(tags.size()))return;
        if(index>0){toggleTagFilter(tags[index-1]);return;}
        libraryFilter.tag.clear();libraryFilter.selectedTags.clear();keyboardPatchId.clear();
        resultOffset=0;browserStatus.clear();refreshBrowser();
    });
    showScrollableLibraryMenu(caller,caller->GetRECT());
}

void Synth1X::showAutomaticTagMenu(IControl* caller)
{
    const auto found=std::find_if(catalog.patches().begin(),catalog.patches().end(),
        [&](const auto& patch){return patch.id==selectedPatchId;});
    if(found==catalog.patches().end())return;
    const auto id=found->id;
    const auto tags=found->visibleAutomaticTags();
    presetMenu=std::make_unique<IPopupMenu>("Automatic tags");
    if(tags.empty())presetMenu->AddItem("No automatic tags",-1,IPopupMenu::Item::kDisabled);
    else for(const auto& tag:tags)presetMenu->AddItem(("Hide: "+tag).c_str());
    const int restoreIndex=presetMenu->NItems();
    presetMenu->AddItem("Restore hidden tags",-1,found->hiddenTags.empty() ? IPopupMenu::Item::kDisabled : 0);
    presetMenu->SetFunction([this,id,tags,restoreIndex](IPopupMenu* menu) {
        const int index=menu->GetChosenItemIdx();
        std::string error;
        if(index==restoreIndex)catalog.restoreAutomaticTags(id,error);
        else if(index>=0 && index<static_cast<int>(tags.size()))catalog.hideAutomaticTag(id,tags[index],error);
        else return;
        browserStatus=error;refreshBrowser();
    });
    showScrollableLibraryMenu(caller,caller->GetRECT());
}

void Synth1X::showProgramAssignment(IControl* caller, const std::string& id, float x, float y)
{
    std::array<std::string, synth1x::host::PatchCatalog::programCount> assignments;
    if (!catalog.readProgramAssignments(assignments, browserStatus)) {
        refreshBrowser();
        return;
    }

    presetMenu = std::make_unique<IPopupMenu>("Assign Program Change");
    const std::weak_ptr<int> lifetime = dialogLifetime;
    presetMenu->SetFunction([this, lifetime, id](IPopupMenu* menu) {
        if (lifetime.expired())
            return;
        if (auto* item = menu->GetChosenItem()) {
            const int number = item->GetTag();
            if (catalog.assignProgram(number, id, browserStatus))
                browserStatus = "Assigned PC " + std::to_string(number) + ".";
            refreshBrowser();
        }
    });
    constexpr int pageSize = 16;
    for (int first = 0; first < static_cast<int>(assignments.size()); first += pageSize) {
        auto* range = new IPopupMenu();
        const auto title = "PC " + std::to_string(first) + "-" + std::to_string(first + pageSize - 1);
        presetMenu->AddItem(title.c_str(), range);
        for (int number = first; number < first + pageSize; ++number) {
            std::string label = "PC " + std::to_string(number);
            const auto& assigned = assignments[number];
            if (!assigned.empty())
                label += assigned == id ? " (this sound)" : " (assigned)";
            range->AddItem(new IPopupMenu::Item(label.c_str(),
                assigned.empty() ? 0 : IPopupMenu::Item::kDisabled, number));
        }
    }
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, IRECT(x,y,x+1,y+1));
}

void Synth1X::showProgramAssignments(IControl* caller)
{
    std::array<std::string, synth1x::host::PatchCatalog::programCount> assignments;
    if (!catalog.readProgramAssignments(assignments, browserStatus)) {
        refreshBrowser();
        return;
    }

    presetMenu = std::make_unique<IPopupMenu>("Program Change");
    const std::weak_ptr<int> lifetime = dialogLifetime;
    for (int number = 0; number < static_cast<int>(assignments.size()); ++number) {
        const auto& id = assignments[number];
        if (id.empty())
            continue;
        const auto* patch = catalog.findPatch(id);
        const std::string name = patch ? patch->patch.name : "Unavailable sound";
        const std::string label = "PC " + std::to_string(number) + " - " + name;
        auto* actions = new IPopupMenu();
        actions->AddItem("Remove assignment");
        presetMenu->AddItem(label.c_str(), actions);
        actions->SetFunction([this, lifetime, number, id](IPopupMenu* menu) {
            if (lifetime.expired() || !menu->GetChosenItem())
                return;
            if (catalog.clearProgram(number, id, browserStatus))
                browserStatus = "Removed PC " + std::to_string(number) + ".";
            refreshBrowser();
        });
    }
    if (!presetMenu->NItems())
        presetMenu->AddItem("No Program Change assignments", -1, IPopupMenu::Item::kDisabled);
    showScrollableLibraryMenu(caller, caller->GetRECT());
}

void Synth1X::stepPatch(int direction)
{
    const auto matches = catalog.search(libraryFilter);
    if (matches.empty())
        return;
    const auto found = std::find_if(matches.begin(), matches.end(),
                                    [&](auto i) { return catalog.patches()[i].id == selectedPatchId; });
    const auto next =
        found == matches.end()
            ? (direction > 0 ? 0 : static_cast<int>(matches.size()) - 1)
            : (static_cast<int>(found - matches.begin()) + direction + static_cast<int>(matches.size())) %
                  static_cast<int>(matches.size());
    resultOffset = synth1x::editor::editorLibraryReveal(matches.size(),visibleRows,resultOffset,next);
    loadLibraryPatch(matches[next]);
}

void Synth1X::loadLibraryPatch(std::size_t index)
{
    // A scan builds a separate catalog. Audition can use the current snapshot
    // while the worker checks the filesystem.
    if (index >= catalog.patches().size() ||
        (libraryJob.valid() && libraryOperation != synth1x::editor::LibraryOperation::scan))
        return;
    loadCatalogPatch(catalog.patches()[index]);
    refreshBrowser();
}
void Synth1X::showImportConflict()
{
    if (importPrompt && !conflictShown && GetUI())
    {
        std::unique_lock lock(importPrompt->mutex);
        if (importPrompt->alive && importPrompt->pending && !importPrompt->answered)
        {
            conflictShown = true;
            auto prompt = importPrompt;
            auto filename = prompt->path.filename().u8string();
            lock.unlock();
            browserStatus = "File already exists: " + std::string(filename.begin(), filename.end());
            conflictMenu = std::make_unique<IPopupMenu>("File already exists");
            for (auto text : {"Replace", "Keep both", "Skip", "Replace all", "Keep both for all", "Skip all"})
                conflictMenu->AddItem(text);
            conflictMenu->SetFunction(
                [this, prompt](IPopupMenu *menu)
                {
                    int i = menu->GetChosenItemIdx();
                    {
                        std::lock_guard lock(prompt->mutex);
                        prompt->choice = {i < 0 ? synth1x::host::FileConflict::skip
                                                : static_cast<synth1x::host::FileConflict>(i % 3),
                                          i < 0 || i >= 3};
                        prompt->answered = true;
                        prompt->pending = false;
                    }
                    prompt->changed.notify_all();
                    conflictShown = false;
                });
            if (auto *caller = GetUI()->GetControlWithTag(bankTag))
                GetUI()->CreatePopupMenu(*caller, *conflictMenu, caller->GetRECT());
            refreshBrowser();
        }
    }
}

void Synth1X::pollLibraryJob()
{
    if (libraryJob.valid() && libraryJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            applyLibraryJob(libraryJob.get());
        }
        catch (const std::exception &e)
        {
            browserStatus = e.what();
        }
        refreshBrowser();
    }
    if (catalogReloadPending && !libraryJob.valid())
    {
        catalogReloadPending = false;
        reloadCatalog();
    }
}

void Synth1X::applyLibraryJob(LibraryJob result)
{
    using synth1x::editor::LibraryOperation;
    const auto operation = libraryOperation;
    const bool current = result.generation == browserGeneration.load();
    const bool succeeded = result.error.empty() &&
        (operation != LibraryOperation::addSounds || result.imported.catalogReady);
    if (succeeded)
    {
        catalog = std::move(result.catalog);
        catalogLoaded = true;
    }
    switch (operation)
    {
        case LibraryOperation::scan:
            if (!succeeded)
                browserStatus = result.error;
            break;
        case LibraryOperation::addSounds:
            if (current)
            {
                libraryFilter = {};
                libraryFilter.registeredOnly = true;
                libraryFilter.registeredIds = std::move(result.imported.ids);
                resultOffset = 0;
                browserStatus = std::to_string(result.imported.imported) + " files registered, " +
                    std::to_string(result.imported.skipped) + " skipped, " +
                    std::to_string(result.imported.errors.size()) + " errors";
                if (!result.imported.errors.empty())
                    browserStatus += " / " + result.imported.errors.front();
            }
            else
                browserStatus = result.error;
            break;
        case LibraryOperation::saveSound:
            if (!current || !succeeded)
            {
                browserStatus = result.error;
                break;
            }
            selectedPatchId = result.savedId;
            currentPatch = result.savedName;
            browser.baseline = result.savedBaseline;
            libraryFilter = {};
            resultOffset = 0;
            browserStatus = "Preset saved.";
            {
                const auto matches = catalog.search(libraryFilter);
                for (std::size_t i = 0; i < matches.size(); ++i)
                    if (catalog.patches()[matches[i]].id == selectedPatchId)
                    {
                        resultOffset = synth1x::editor::editorLibraryReveal(matches.size(), visibleRows, resultOffset, i);
                        break;
                    }
            }
            showPage(synth1x::editor::editorLibraryView);
            break;
    }
}

void Synth1X::reloadCatalog(bool queueIfBusy)
{
    if (libraryJob.valid())
    {
        if(queueIfBusy){catalogReloadPending=true;browserStatus="Refresh queued...";refreshBrowser();}
        return;
    }
    auto root = catalog.root();
    libraryJob = std::async(std::launch::async,
                            [root]
                            {
                                LibraryJob job{synth1x::host::PatchCatalog(root)};
                                job.catalog.reload(job.error);
                                return job;
                            });
    libraryOperation=synth1x::editor::LibraryOperation::scan;
    browserStatus.clear();
    refreshBrowser();
}
void Synth1X::importBank(const std::string &path)
{
    if (libraryJob.valid())
    {
        browserStatus = "Please wait for the current library operation.";
        refreshBrowser();
        return;
    }
    importPrompt = std::make_shared<ImportPrompt>();
    auto prompt = importPrompt;
    auto root = catalog.root();
    auto generation = browserGeneration.load();
    libraryJob = std::async(
        std::launch::async,
        [root, path, prompt, generation]
        {
            LibraryJob job{synth1x::host::PatchCatalog(root)};
            job.generation = generation;
            job.catalog.registerFiles(
                path,
                [prompt](const auto &destination)
                {
                    std::unique_lock lock(prompt->mutex);
                    if (!prompt->alive)
                        return synth1x::host::ConflictChoice{synth1x::host::FileConflict::skip, true};
                    prompt->path = destination;
                    prompt->pending = true;
                    prompt->answered = false;
                    prompt->changed.wait(lock, [&] { return prompt->answered || !prompt->alive; });
                    return prompt->alive
                               ? prompt->choice
                               : synth1x::host::ConflictChoice{synth1x::host::FileConflict::skip, true};
                },
                job.imported);
            return job;
        });
    libraryOperation=synth1x::editor::LibraryOperation::addSounds;
    browserStatus = "Registering sounds...";
    showPage(synth1x::editor::editorLibraryView);
    refreshBrowser();
}
void Synth1X::showImportMenu(IControl *caller)
{
    presetMenu = std::make_unique<IPopupMenu>("Add sounds");
    presetMenu->AddItem("Add .zip/.rar/.7z bank or .sy1 file...");
    presetMenu->AddItem("Add folder of .sy1 sounds...");
    presetMenu->AddItem("Open library folder");
    presetMenu->AddItem("Save");
    presetMenu->AddItem("Save as...");
    presetMenu->AddItem("Show all sounds");
    presetMenu->AddItem("Choose library folder...");
    presetMenu->SetFunction(
        [this](IPopupMenu *menu)
        {
            const auto index = menu->GetChosenItemIdx();
            if (index == 0)
                promptForBank();
            else if (index == 1)
                promptForFolder();
            else if (index == 3)
                savePreset(false);
            else if (index == 4)
                savePreset(true);
            else if (index == 5)
            {
                libraryFilter = {};
                resultOffset = 0;
                browserStatus.clear();
                refreshBrowser();
            }
            else if (index == 6)
            {
                if (libraryJob.valid())
                    return;
                const std::weak_ptr<int> lifetime = dialogLifetime;
                WDL_String path;
                GetUI()->PromptForDirectory(
                    path,
                    [this, lifetime](const WDL_String &, const WDL_String &chosen)
                    {
                        if (lifetime.expired() || !chosen.GetLength())
                            return;
                        auto root = std::filesystem::path(
                            std::u8string(chosen.Get(), chosen.Get() + chosen.GetLength()));
                        if (synth1x::host::PresetFiles::configureDirectory(root, browserStatus))
                        {
                            catalog = synth1x::host::PatchCatalog(root);
                            catalogLoaded = false;
                            libraryFilter = {};
                            selectedPatchId.clear();
                            resultOffset = 0;
                            reloadCatalog();
                        }
                        refreshBrowser();
                    });
            }
            else if (index == 2)
            {
                try
                {
                    if (catalog.root().empty())
                        throw std::runtime_error("The user library folder is unavailable.");
                    std::filesystem::create_directories(catalog.root());
                    const auto utf8 = catalog.root().u8string();
                    const std::string path(utf8.begin(), utf8.end());
                    WDL_String folder(path.c_str());
                    if (auto *ui = GetUI())
                        if (!ui->RevealPathInExplorerOrFinder(folder))
                            browserStatus = "The library folder could not be opened.";
                }
                catch (const std::exception &e)
                {
                    browserStatus = e.what();
                }
                refreshBrowser();
            }
        });
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, caller->GetRECT());
}
void Synth1X::promptForFolder()
{
    auto *graphics = GetUI();
    if (!graphics)
        return;
    const std::weak_ptr<int> lifetime = dialogLifetime;
    WDL_String path;
    graphics->PromptForDirectory(path,
                                 [this, lifetime](const WDL_String &, const WDL_String &chosen)
                                 {
                                     if (lifetime.expired() || !chosen.GetLength())
                                         return;
                                     importBank(chosen.Get());
                                 });
}
void Synth1X::promptForBank()
{
    auto *graphics = GetUI();
    if (!graphics)
        return;
    const std::weak_ptr<int> lifetime = dialogLifetime;
    WDL_String file, path;
    graphics->PromptForFile(file, path, EFileAction::Open, "sy1 zip rar 7z",
                            [this, lifetime](const WDL_String &chosen, const WDL_String &)
                            {
                                if (lifetime.expired() || !chosen.GetLength())
                                    return;
                                importBank(chosen.Get());
                            });
}
void Synth1X::savePreset(bool saveAs)
{
    if (libraryJob.valid())
    {
        browserStatus = "Please wait for the current library operation.";
        refreshBrowser();
        return;
    }
    const auto found = std::find_if(catalog.patches().begin(), catalog.patches().end(),
                                    [this](const auto &p) { return p.id == selectedPatchId; });
    if (!saveAs && found != catalog.patches().end() && !found->file.empty() && found->archiveEntry.empty())
    {
        writePreset(found->file, true, currentPatch);
        return;
    }
    auto *ui = GetUI();
    if (!ui)
        return;
    std::string name = currentPatch;
    for (auto &c : name)
        if (std::string("/\\:*?\"<>|\r\n").find(c) != std::string::npos)
            c = '_';
    const auto root = catalog.root().u8string();
    WDL_String file((name + ".sy1").c_str()), path(reinterpret_cast<const char *>(root.c_str()));
    const std::weak_ptr<int> lifetime = dialogLifetime;
    const auto generation=browserGeneration.load();
    ui->PromptForFile(
        file, path, EFileAction::Save, "sy1",
        [this, lifetime,generation](const WDL_String &chosen, const WDL_String &)
        {
            if (lifetime.expired() || !chosen.GetLength())
                return;
            if(generation!=browserGeneration.load()) {
                browserStatus="Comparison changed. Choose Save again.";refreshBrowser();return;
            }
            writePreset(std::filesystem::path(std::u8string(chosen.Get(), chosen.Get() + chosen.GetLength())),
                        false);
        });
}
void Synth1X::writePreset(const std::filesystem::path &destination, bool confirm, const std::string &title)
{
    const auto generation=browserGeneration.load();
    auto save = [this, destination, title,generation]
    {
        if (libraryJob.valid())
            return;
        if(generation!=browserGeneration.load()) {
            browserStatus="Comparison changed. Choose Save again.";refreshBrowser();return;
        }
        try
        {
            auto patch = synth1x::parseContinuousPatch(processor.savePatch());
            auto stem = destination.stem().u8string();
            const std::string name = title.empty() ? std::string(stem.begin(), stem.end()) : title;
            patch.name = name;
            const auto text = synth1x::formatContinuousPatch(patch);
            const auto root = catalog.root();
            const auto relative = std::filesystem::weakly_canonical(destination)
                                      .lexically_relative(std::filesystem::weakly_canonical(catalog.root()));
            auto snapshot = browser;
            snapshot.capture(processor);
            libraryJob = std::async(std::launch::async,
                                    [root, relative, text, name, snapshot, generation]
                                    {
                                        LibraryJob job{synth1x::host::PatchCatalog(root)};
                                        job.generation = generation;
                                        job.savedName = name;
                                        job.savedBaseline = snapshot.baseline;
                                        job.catalog.saveFile(relative, text, true, job.savedId, job.error);
                                        return job;
                                    });
            libraryOperation=synth1x::editor::LibraryOperation::saveSound;
            browserStatus = "Saving preset...";
            refreshBrowser();
        }
        catch (const std::exception &e)
        {
            browserStatus = e.what();
            refreshBrowser();
        }
    };
    if (confirm && std::filesystem::exists(destination))
    {
        const std::weak_ptr<int> lifetime = dialogLifetime;
        GetUI()->ShowMessageBox("Replace the existing preset file?", "Save preset", kMB_YESNO,
                                [save, lifetime](EMsgBoxResult result)
                                {
                                    if (!lifetime.expired() && result == kYES)
                                        save();
                                });
    }
    else
        save();
}

void Synth1X::showSaveMenu(IControl *caller)
{
    presetMenu = std::make_unique<IPopupMenu>("Save preset");
    presetMenu->AddItem("Save");
    presetMenu->AddItem("Save as...");
    presetMenu->SetFunction(
        [this](IPopupMenu *menu)
        {
            int i = menu->GetChosenItemIdx();
            if (i >= 0)
                savePreset(i == 1);
        });
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, caller->GetRECT());
}
