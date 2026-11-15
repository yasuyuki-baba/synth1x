# A/B sound comparison

Two sound displays remain in the header on Synth, MIDI and Library. A and B each
retain a complete sound/loading snapshot, its patch name, library identity and
load/save edit baseline. The selected side has an accent border. Both names and
edit markers stay visible; long labels use measured ellipsis and full tooltips.

Click the other display to switch the sounding side. Click the selected display
to open Library. Loading a Library sound, using the header's previous/next arrows
or editing a parameter changes only the selected side. The other side is retained
until an explicit copy replaces it. A > B replaces B with A, including unsaved
edits; B > A does the reverse. Copying into the selected side immediately applies
the copied sound without changing the selected letter. Copying into the other
side leaves the sounding processor unchanged.

For patch selection, keep a reference in A, select B and audition candidates from
Library. For sound design, copy A to B, edit B and switch between the original and
edited versions. Comparison itself does not write a patch file. Save/Save as
continues to apply to the selected side and its source file.

Search, bank, tag and favorite filters, scroll position and editor view remain
shared. A/B switches clear the keyboard browsing candidate without changing those
filters. Tags and favorites remain library metadata, rather than per-slot sound
state. Oversampling is shared: switching/copying never changes the requested
quality factor. Master volume remains part of each sound; comparison does not
normalize levels. Slots are disabled during asynchronous library operations. A pending save dialog
is cancelled if the comparison changes before confirmation.

DAW and standalone sessions retain the selected letter and both sounds, including
unsaved edits and their baselines, without requiring external patch files. Older
single-sound sessions initialize both sides from their restored sound and select
A. Both sound payloads and their metadata must validate before recall changes the
processor, browser or comparison. The wrapper v3 envelope embeds the sounding
session plus the inactive sound; v1/v2 remain readable. Individual .sy1 exports
contain the selected sound and do not include the comparison workspace.

Snapshots contain controls and source-loading context. Running oscillator phases,
held-note ownership, effect buffers and RNG execution history are not captured.
Switches apply through the existing state-loading and framework host-notification
paths; comparison is not an audio crossfade or sample-identical replay of a
previous performance. Header controls are UI actions, not new host parameter IDs.
