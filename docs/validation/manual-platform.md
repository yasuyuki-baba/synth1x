# Optional manual platform checks

Portable tests exercise the actual SDK-independent core and bridge. They do not
instantiate an iPlug2 wrapper, editor, operating-system audio device or DAW.
These are reference steps for separate manual work on intended release platforms.
Actual GUI interaction, DAW sessions and physical devices are outside automated-test
scope. This page is not an automated coverage checklist or a pending-test backlog.
Use it when manual platform checks are requested, and record only what actually ran.

## Build and load

Check out the pinned iPlug2 SDK and required format dependencies as described in
[the build boundary](../desktop-build.md). Build Release with the root presets:

| Platform | Configure/build presets | Targets to load |
| --- | --- | --- |
| Windows | `windows`, `windows-app` | VST3, CLAP, standalone APP |
| macOS | `macos`, `macos-app` | VST3, CLAP, AUv2, standalone APP |
| Linux (experimental) | `linux`, `linux-app` | VST3, CLAP, standalone APP |

For example, run `cmake --preset windows`, then
`cmake --build --preset windows`. Repeat with the applicable APP/macOS preset.
A successful compile is a build result; verify each resulting format in a host.
On macOS verify Apple Silicon products with a native arm64 host. Record the product and SDK commits, compiler, architecture, format,
host/version, sample rate, block size and actual result outside this specification.

## Host acceptance

For every plugin format, use a fresh host session and a self-authored patch or the
explicitly imported bank. Record host input/output and steps when a failure occurs.

| Scenario | Required result |
| --- | --- |
| Scan, instantiate and open/close editor repeatedly | Host recognizes the format; editor and audio remain functional |
| Play note/chord, release keys, use sustain and release pedal | Audible notes; pedal holds released keys; release finishes without stuck notes |
| Record/play automation of source control and master volume | Correct named control moves and audible change; reopening the editor shows host state |
| Save session after editing and selecting a file-based patch | Reopened session restores controls and selected patch without the original bank file |
| Reload session, render a new note from stopped transport | Restored source produces the same cold-start sound under equal input/setup; running DSP history is not serialized |
| Change tempo with a tempo-synced patch | Host tempo reaches the instrument without non-finite output or a crash |
| Stop/restart playback and change supported rate/buffer configuration | Instrument remains playable; normal note release and effect tails remain valid |
| Multiple instances and repeated instance deletion | Edits and state belong to their own instance; host remains stable |

Compare cold-start audio only after equal prepare/loading/MIDI history and equal
callback configuration. Arbitrary callback partitions can affect native receiver
and gain boundaries. Host plugin-identity compatibility is a separate check from
readability of an old state chunk.

For standalone APP, repeat editor interaction and MIDI/pedal checks with a real
MIDI/audio device, verify device selection and sample-rate changes, and close and
reopen the application. Do not infer device latency or hard real-time guarantees
from portable tests or successful offline rendering.

## Native Windows APP capture

`tools/capture_iplug_wine.py` captures a built Windows APP under Wine with audio
and MIDI I/O disabled. It requires the built PE, Wine/Xvfb/X11 and ImageMagick.
Use its `--help` for explicit executable, Wine, prefix and output paths. Preserve
runtime trust/proxy configuration and use a fresh output file.

A captured native frame checks that the actual APP editor can initialize and
paint in the specified environment. It does not establish DAW plugin behavior,
audio-device operation, native macOS operation or unrestricted GUI interactions.

See the [GUI contract](../gui/README.md) and [desktop build boundary](../desktop-build.md).
Record the executable identity and capture conditions with any reported frame.

## Library and keyboard interaction

For a library with more banks than fit on screen, open the bank selector and check
platform-menu scrolling, wheel/trackpad behavior and selection near both ends.
Verify indentation, disabled parent headings, All banks and This folder, then open
another menu to confirm custom popup rendering is restored. Check both Windows
and macOS; portable presentation tests do not exercise native menu interaction.

Refresh a library with named Bass/Pad sounds and feature-rich patches. Check that
manual tags use ordinary text and inferred tags use muted text in the same row.
Filter/search both kinds without changing the sounding patch. Use Auto tags to
hide and restore a suggestion, reopen the editor, and confirm persistence. Enter
the same tag manually with different case; it should appear once in the manual
color. Check narrow/overflowing tag columns and selection/readability on both OSes.

Toggle multiple tags in the checked tag menu and verify AND results, the selection
count/tooltip, All tags reset and DAW session recall. Refresh/reopen a large
library twice, then add, remove and replace loose/ZIP sounds externally and
refresh. Check that the updated list and tags reflect the current sources while
the sounding patch and manual/hidden-tag overrides remain intact.

With a filtered library, use Ctrl/Command+F, type a query and confirm it with
Enter. Move with arrows, pages and Home/End: the candidate outline should move
without changing the sounding title. Enter should load it; Escape should return
to the sounding entry if still in the results. Check empty results, invalid
sources, catalog refresh, mouse selection, software-keyboard performance, and
ordinary text editing/host shortcuts while the editor is unfocused. On the
standard keyboard, click white/black keys at different heights and drag between
keys; verify note release on mouse-up. Check PC note release on hiding, focus
loss and editor close. Mouse, PC and external notes share ordinary MIDI ordering
rather than independent ownership of the same pitch.

Scroll the Library with wheel/trackpad input and drag the scrollbar thumb from
top to bottom; click the track to jump. Check a short/empty list, fractional
rows, first/last bounds, clipped drawing and row/favorite hit tests after scrolling.
Move the keyboard candidate across either viewport edge: reveal only the needed
rows with a short transition, keeping the sounding patch unchanged until Enter.
Check that refresh/filter changes clamp the viewport and session recall restores
the first visible row after scanning. No page buttons should remain.

Use the bank selector arrows alongside its menu. Check identical selectable
ordering, parent folders with direct sounds, All banks and disabled end buttons.
Keywords/tags/favorites and the sounding patch must survive a bank change, while
the registration-only restriction and scroll/candidate reset.

At the standard editor size, check the shared larger type across Synth, MIDI,
Library, header/footer and popup menus. Exercise long choices, long patch/bank
names and tags, extreme unit readouts, Chorus lower-row labels and the compact
filter envelopes. Text should remain inside its control without reducing the
choice font. Confirm numeric-entry hit areas and menu/full-value tooltips.

Resize the standalone window from its frame horizontally, vertically and at a
corner, then shrink it again. Text and controls should scale together at a stable
aspect ratio and fit both available dimensions within the supported scale range.
Check wide and tall windows, maximization/restoration and tiling layouts: the
parent should retain its requested bounds, with unused space when its aspect
ratio differs from the canvas. Test the editor corner handle too. Open/close
Keyboard at a changed scale and exercise popup, numeric-entry and note hit areas.
In a DAW, resize the host-owned view and check the same uniform fit without
resize-request loops. Repeat on Windows, macOS and Linux and on displays with
different platform pixel scales.

In Synth, MIDI and Library, verify Scale stays below the header logo, the sound
count aligns below the patch display, and the footer orders Oversampling /
Keyboard. Enlarge the view beyond the screen and reduce it through the header
Scale control. Select 75/100/125% from its arrow, then click the numeric field
and enter `112.5%`. Enter commits; Escape and invalid inputs retain the scale.
Check the actual readout after ordinary resizing, maximization and Keyboard
open/close. Requests exceeding the work area or rejected by a plugin host must
fit accepted bounds and display the accepted scale. Close/reopen the editor in
the same instance and confirm its scale is retained. Sound automation, patch
exports and project sound state must not acquire a display-scale parameter.

The bridge caches mapped source snapshots by a published parameter revision and
random-history validity. Same-value setters, loads and recall publish revisions;
history notifications, pending filter edits, phase resets and master volume remain
callback-visible. PlaybackBehavior compares cached callbacks with explicit refresh.

For non-gating DSP profiling, build `Synth1XRenderProfile` and run:

```sh
./build-oos-core/Synth1XRenderProfile render-profile.csv --blocks 1024 --rounds 3
```

The CSV records thread CPU/wall means and a final-block hash. These offline
measurements do not certify device latency or a hard real-time deadline.

## Palette window

From Synth, MIDI and Library, open **Palette...** in the footer. Verify that a
second click raises the same modeless window and that the main editor remains
usable. Verify all General colors are visible together, with the selected cell outlined;
switch between cells and confirm the adjacent inspector follows the selection.
Verify the left/right pairs match the palette order in the GUI contract.
Switch between General and groups: all cells must retain their positions.
Cells restricted to General must be dimmed and unselectable in groups. Only explicitly
set group colors show Custom; inherited and unavailable cells have no extra
labels. Swatches, HEX values and Custom badges must update immediately after
editing, Use General and Reset group.
Change General panel/text/control colors with the picker, HEX and sliders,
including 0%, 50% and 100% opacity. Check the Synth controls, MIDI, Library,
popup menus and software keyboard. Repeated redraws must not accumulate opacity.

Override one group's panel/text/heading/knob colors; other groups must retain
General. Verify **Use General**, **Reset group** and the default Filter overrides.
With both General edits and group overrides, click **Default colors**: Cancel,
Escape and Return must preserve the preview. **Restore defaults** must reset all
scopes immediately, without saving until Apply. Cancel/discard after resetting
must still restore the opening palette. An already-default palette must not show
a reset confirmation.
Make text transparent and confirm Palette's fixed entry button, edit window and
discard confirmation remain readable. Change a color, then Cancel or close the
palette window: Keep editing must retain both the window and preview; Discard
changes must restore the opening palette and close. Escape/Return in the
confirmation must keep editing, and repeated close requests must not stack
prompts. Verify the title-bar close does not hide the palette before confirmation
on each platform. Unchanged colors, selection-only actions and edits restored to
their opening values close without a prompt. Verify an uncommitted valid HEX or
opacity entry is included when closing through the title bar. Closing the main
editor must close its child and restore the opening palette directly. Apply a
change, reopen the editor and open a second instance; verify the
saved preference loads, while already-open instances retain their appearance.
Switch sounds/A/B and recall DAW state without changing the active palette.

Check the palette window at OS DPI scales used on the platform, including closing
the parent editor while the window or native picker is open. A failure to write
preferences must show an error and leave editing/cancellation available. Use a
temporary user-data directory for destructive preference-file experiments.


## Palette import/export

From the palette window, export a draft with transparent General colors, several
group overrides and inherited colors. Include a valid HEX/RGBA value still being
typed when clicking Export. Verify the chosen file contains the complete draft,
while the active preference file remains unchanged when exporting elsewhere.
Export must leave the edit unsaved; Cancel/title-bar close still asks before
discarding it. Cancelling either file chooser must leave the preview unchanged.

Import the exported file in another editor, verify all colors, alpha values and
Custom badges, then Cancel/discard: restore the opening palette. Repeat with
Apply and reopen to verify activation. Import malformed, oversized and unsupported
XML files and verify the preview remains intact with an error message. Test paths
with non-ASCII characters. Export with and without an extension; verify `.xml`
when omitted. Export to an existing file and test both cancelling and confirming
replacement. A failed export must preserve editing and the current preference.


## Background images

In the palette editor, choose PNG and JPEG images, including a Unicode filename.
Verify immediate main-surface preview, fixed-color palette/recovery controls and
centered cover without stretching at different window scales and with Keyboard
shown/hidden. Make General Panel/Header and Filter Panel half-transparent and
fully transparent; the image must show through panels without opacity accumulating
on redraw. A transparent header must reveal only the Background color; the image
must begin at the header's bottom edge. Switch Synth/MIDI/Library and verify the background remains available.

Cancel the chooser and reject a corrupt/unsupported or oversized image: preserve
the current preview. Check the 8 MiB, 8192-pixel-side and 16-megapixel limits.
Remove image, then Cancel/discard and verify restoration. Default colors must
reset colors while retaining the image; an image alone must not trigger the color
reset confirmation. Image selection/removal must participate in close confirmation.
Apply an image, delete the original source and reopen: the saved background must
still appear. Export/import on another settings directory must retain the image;
import a color-only palette and verify removal. Cancel/discard an import must
restore the opening image. Malformed embedded image data must preserve the current
appearance and show an error. Use temporary settings directories for these checks.
