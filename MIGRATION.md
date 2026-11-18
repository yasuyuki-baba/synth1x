# Upgrading from Synth2 0.1.x to Synth1X 0.2.x or later

I've decided to rename the plugin from Synth2 to Synth1X to avoid confusion.
I'm sorry for the extra work this creates for anyone already using Synth2.

This upgrade requires manual migration. Starting with 0.2.x, the plugin and
standalone filenames change from `Synth2` to `Synth1X`, and user data and
standalone settings use new directories. Copy your existing library and settings
to the new locations.

The AUv2 subtype and CLAP plugin ID also change. Replace existing AUv2 and CLAP
instances in your DAW projects after exporting their patches. The VST3 class ID
stays the same.

On macOS, the plugin and standalone bundle IDs now end in `.Synth1X`.

## Prepare your projects

Keep a copy of your DAW projects, the old Synth2 binaries and the old user data
folders. While you can still open your projects with Synth2, prepare any AUv2 or
CLAP instances you want to migrate:

1. In each instance, use **Save → Save as...** to export its sound as a `.sy1`
   file. If you use both A/B sounds, select and save each side separately.
2. Note any DAW automation assignments and shared instance settings you want to
   reproduce, such as oversampling. A `.sy1` file carries one sound. The DAW
   project carries automation and the full plugin state.
3. Close the DAW and the standalone app before copying data or replacing files.

## Copy your library and settings

Before opening or scanning Synth1X, copy the contents of the old user data
folder into the new one. This brings over the default library, tags, favorites,
palette and standalone patch state. If both folders already contain data,
compare overlapping files before copying.

| Platform | Old user data folder | New user data folder |
| --- | --- | --- |
| Windows | `%APPDATA%/Synth2` | `%APPDATA%/Synth1X` |
| macOS | `~/Library/Application Support/Synth2` | `~/Library/Application Support/Synth1X` |
| Linux | `~/.local/share/Synth2` | `~/.local/share/Synth1X` |

On Linux, use `$XDG_DATA_HOME` in place of `~/.local/share` if set.

To bring over standalone audio and MIDI device selections, copy `settings.ini`
from the old folder to the new one before the first launch:

| Platform | Old standalone settings folder | New standalone settings folder |
| --- | --- | --- |
| Windows | `%LOCALAPPDATA%/Synth2` | `%LOCALAPPDATA%/Synth1X` |
| macOS | `~/Library/Application Support/Synth2` | `~/Library/Application Support/Synth1X` |
| Linux | `~/.config/Synth2` | `~/.config/Synth1X` |

On macOS, the user data copy above includes this file. On Linux, use an absolute
`$XDG_CONFIG_HOME` in place of `~/.config` if set.

If you picked a custom library location, the copied settings retain its saved
absolute path. If you moved the library, reselect it with **Add... → Choose
library folder...** after opening the editor.

Synth1X can open existing Synth2 palette files. Keep the original XML if you
also use Synth2, which expects the original palette tag.

Copy existing library tag settings as they are. Synth1X reads Synth2's `.tags`
files and uses a new header when saving tag edits. If you return to Synth2,
use the original tag files from your backup.

## Install and reopen

Move the old `Synth2` plugins out of your DAW's scan folders, install the
matching `Synth1X` products using the [platform installation instructions](INSTALL.md)
and rescan.
Open a copy of your project to check playback and automation before saving it.

| Format | What to do in an existing project |
| --- | --- |
| VST3 | The class ID stays the same. Rescan and check that the DAW opens the existing instance with its saved state. |
| CLAP | The ID changes from `io.github.yasuyuki-baba.Synth2` to `io.github.yasuyuki-baba.Synth1X`. Add a Synth1X instance, load the exported patch and reassign automation. |
| AUv2 | The subtype changes from `Syn2` to `Sy1X`. Add a Synth1X instance, load the exported patch and reassign automation. |

If you exported both A/B sounds, select each side in the new instance and load
the corresponding patch.

Launch the new standalone app from its new file and update any shortcuts. On
macOS it has a new app identity and virtual MIDI ports named `To Synth1X` and
`From Synth1X`. Select the new names in apps that use those ports. Check the
audio and MIDI selections on the first launch.

To return to Synth2, close the DAW and app, swap the plugin files in the scan
folders back to the old versions, then rescan. Open the unmodified project
backup with the old user data folders in place.
