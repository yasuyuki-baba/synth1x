# Installing Synth1X

Download the file for your operating system from the **Assets** section of
the [latest Synth1X release](https://github.com/yasuyuki-baba/synth1x/releases/latest).

| Platform | Download |
| --- | --- |
| Windows x64 | `Synth1X-windows.zip` |
| macOS Apple Silicon | `Synth1X-macos.tar.gz` |
| Linux x86_64 | `Synth1X-linux.tar.gz` |

The extracted package contains a `Synth1X` folder with the plugin products,
standalone application, `dependencies.json`, `LICENSE` and `notices/`.
Products are directly inside `Synth1X/`, not in an `out/` subfolder.
`dependencies.json` identifies the source and SDK revisions for that build.

Close your DAW and Synth1X before installing or updating. Copy each complete
product, keeping its original name and contents. When updating, replace the old
bundle rather than merging folders. Install only formats your DAW supports.
Keep the package's license and notice files with your downloaded distribution.

## Upgrading from Synth2

Upgrading from Synth2 0.1.x to Synth1X 0.2.x or later requires manual migration.
Follow the [migration guide](MIGRATION.md) before replacing your Synth2 installation.

## Linux

Linux packages target x86_64 and are built on Ubuntu 24.04 (glibc 2.39). Use that
release or a compatible newer distribution; ARM Linux and older glibc systems are
not covered by these packages. Extract the tar.gz on Linux to preserve executable
permissions and symlinks. The editor uses X11 and OpenGL 3; Wayland sessions need
XWayland. Runtime libraries include GTK3, FreeType/fontconfig, X11/OpenGL, ALSA,
JACK and PulseAudio. On Ubuntu 24.04, install missing runtime dependencies with:

```sh
sudo apt-get install libgtk-3-0t64 libasound2t64 libjack-jackd2-0 libpulse0 libgl1
```

| Product in `Synth1X/` | Destination folder |
| --- | --- |
| `Synth1X.vst3` (entire bundle) | `~/.vst3/` |
| `Synth1X.clap` | `~/.clap/` |
| `Synth1X` (standalone) | A folder you own, such as `~/.local/opt/Synth1X/` |

Create missing folders. Keep any `Synth1X.clap.resources` directory beside
`Synth1X.clap`, and any `Synth1X.resources` directory beside the standalone binary.
Keep the VST3 bundle intact, including `Contents/x86_64-linux/Synth1X.so` and its
resources. Run the standalone binary from its extracted folder; audio requires a
working ALSA device or JACK server. Use the portable source build on platforms
outside this binary baseline; see the [build guide](https://github.com/yasuyuki-baba/synth1x/blob/main/docs/desktop-build.md).

## macOS

macOS packages require Apple Silicon (arm64) and macOS 13 or later.
Intel Macs are not supported. In Finder, choose **Go > Go to Folder…** and
enter a destination below. `~` means your home folder. Create missing folders;
these user-level locations do not require administrator permission.

| Product in `Synth1X/` | Destination folder |
| --- | --- |
| `Synth1X.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `Synth1X.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |
| `Synth1X.component` (AUv2) | `~/Library/Audio/Plug-Ins/Components/` |
| `Synth1X.app` (standalone) | `~/Applications/` |

Copy the entire bundle into its destination. For example, the installed VST3
should be `~/Library/Audio/Plug-Ins/VST3/Synth1X.vst3`. AUv2 is for hosts such as
Logic Pro; VST3 and CLAP need a host that supports those formats.

### Unsigned builds

The macOS binaries are not signed with a Developer ID or notarized. macOS may
block the application or prevent a DAW from scanning a downloaded plugin.

For the standalone application, try opening it, then use **System Settings >
Privacy & Security > Open Anyway** if macOS offers that option. Follow the
system's prompts to open this application.

If a plugin is blocked by download quarantine and you trust the package's source,
open Terminal and run only the command for each product you installed:

```sh
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Synth1X.vst3"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/CLAP/Synth1X.clap"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/Synth1X.component"
```

These commands remove quarantine only from the specified Synth1X bundles. They
do not sign the products or disable Gatekeeper globally. Restart your DAW and
rescan afterward. Hosts or managed computers that require signed/notarized code
may still reject the build; do not disable system-wide protections to install it.

## Windows

Windows packages target x64. Extract the complete ZIP before
launching or installing products. In File Explorer, copy these products:

| Product in `Synth1X/` | Destination folder |
| --- | --- |
| `Synth1X.vst3` (entire folder) | `C:\Program Files\Common Files\VST3\` |
| `Synth1X.clap` | `C:\Program Files\Common Files\CLAP\` |

These shared plugin folders usually need administrator permission. Create missing
folders as needed. Keep the complete VST3 bundle structure; do not copy only the
binary inside `Contents/x86_64-win/`. If your DAW supports a custom scan folder,
you can instead use a folder you own and configure it in the DAW's search paths.

For standalone use, keep `Synth1X.exe` and any accompanying runtime files together
in a folder you own and launch the executable. You can create a shortcut; no DAW
is required.

### Unsigned builds

The Windows binaries are not code-signed. SmartScreen may display a warning
when launching the standalone application. If you trust the downloaded package,
choose **More info > Run anyway** when that option is offered.

If Windows marks a downloaded file as blocked, right-click it, choose
**Properties**, and select **Unblock > Apply** if shown. You can also unblock the
downloaded ZIP before extracting it. Managed computers may enforce policies that
prevent unsigned software from running.

## Load and play

1. Restart the DAW after copying the plugins.
2. Enable the installed format, check its scan paths and run a plugin rescan.
   If Synth1X previously failed scanning, use the host's option to retry failed
   or blocked plugins.
3. Load **Synth1X** on an instrument/MIDI track. Select the MIDI input, enable
   track monitoring and configure your audio output. In the standalone app,
   select the audio device and MIDI input in its settings.

Synth1X starts with Init and an empty library. To add your first patches:

1. Open the **Library** tab.
2. Choose **Add... → Add .zip/.rar/.7z bank or .sy1 file...** and select your file.
3. Click a patch name in the list to load it, then play a note.

The default oversampling factor is 1x. Higher factors use more CPU and can change
tone and level. Reduce the factor or increase your audio buffer if playback
crackles.

If the plugin is missing, check its installation folder, the host's supported
formats and CPU architecture, and the DAW's scan/validation log. Run Apple Silicon DAWs natively; Intel/Rosetta hosts cannot load these arm64-only
plugins.
For AUv2 discovery problems, logging out and back in can refresh Audio Unit
registration before rescanning.

A successful build is not a guarantee of host/device compatibility. See the
[manual platform checks](https://github.com/yasuyuki-baba/synth1x/blob/main/docs/validation/manual-platform.md) for verification scope.

## Uninstall

Close the DAW and standalone app, then remove the Synth1X products from their
installation folders. Rescan if the DAW still lists the plugin.

Removing the products leaves your user data in place. The default library is in
the `Library` subfolder of the user data directory. A custom library stays in the
folder you selected.

| Platform | User data directory | Standalone device settings file |
| --- | --- | --- |
| Windows | `%APPDATA%\Synth1X\` | `%LOCALAPPDATA%\Synth1X\settings.ini` |
| macOS | `~/Library/Application Support/Synth1X/` | `~/Library/Application Support/Synth1X/settings.ini` |
| Linux | `~/.local/share/Synth1X/` | `~/.config/Synth1X/settings.ini` |

On Linux, `$XDG_DATA_HOME` replaces `~/.local/share` when set. An absolute
`$XDG_CONFIG_HOME` replaces `~/.config` for the standalone settings.
