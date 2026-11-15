#pragma once
#include "EditorLibraryScroll.h"
#include <cmath>
#include <string>

namespace synth2::editor
{
enum class LibraryOperation { scan, addSounds, saveSound };
struct EditorLibraryStatusInput
{
    bool busy=false, ready=false, registeredOnly=false;
    LibraryOperation operation=LibraryOperation::scan;
    std::size_t sounds=0, matches=0, visibleRows=17;
    double offset=0;
};
struct EditorLibraryStatus
{
    std::string summary, matches, emptyTitle, emptyDetail;
};
inline EditorLibraryStatus editorLibraryStatus(const EditorLibraryStatusInput& input)
{
    EditorLibraryStatus result;
    if(input.busy) {
        switch(input.operation) {
            case LibraryOperation::scan:
                result.matches=input.ready ? "Updating library..." : "Loading library...";
                result.emptyDetail="Scanning files and preparing automatic tags.";break;
            case LibraryOperation::addSounds:
                result.matches="Adding sounds...";
                result.emptyDetail="Registering sounds and preparing automatic tags.";break;
            case LibraryOperation::saveSound:
                result.matches="Saving sound...";
                result.emptyDetail="Saving the sound and updating the library.";break;
        }
        result.summary=(input.ready ? std::to_string(input.sounds)+" sounds / " : "")+result.matches;
        result.emptyTitle=result.matches;
    } else if(!input.ready) {
        result.summary=result.matches=result.emptyTitle="Library unavailable";
        result.emptyDetail="Use Refresh to try again. See the status line for details.";
    } else {
        result.summary=(input.registeredOnly ? "Registered / " : "")+std::to_string(input.sounds)+" sounds in library";
        const auto offset=editorLibraryScrollOffset(input.matches,input.visibleRows,input.offset);
        const auto first=input.matches ? static_cast<std::size_t>(offset)+1 : 0;
        const auto last=std::min(static_cast<std::size_t>(std::ceil(offset+input.visibleRows)),input.matches);
        result.matches=std::to_string(first)+" - "+std::to_string(last)+" of "+std::to_string(input.matches)+" sounds";
        result.emptyTitle=input.sounds ? "No matching sounds." : "Your library is empty.";
        result.emptyDetail=input.sounds ? "Clear a filter to see more sounds." : "Use Add... to import patches or sound banks.";
    }
    return result;
}
}
