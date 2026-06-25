# Installers

Double-click installers for Windows. They copy the **already-built** artifacts to the right
places — run them either from a release bundle (installer sitting next to the artifact) or
straight from a local build (they auto-find `..\build\...`).

| Installer | Installs | Location | Admin? |
|-----------|----------|----------|--------|
| `install-app.bat` | Standalone editor `MS2K_Interface.exe` | `%LOCALAPPDATA%\Programs\MS2K Interface` + Start Menu & Desktop shortcuts | No |
| `install-vst3.bat` | Plugin `MS2K Editor.vst3` | `%CommonProgramFiles%\VST3` (the folder DAWs scan) | Yes — self-elevates |

## Use
1. Build the targets first (or grab them from a GitHub Release):
   ```sh
   cmake --build build --target MS2K_Interface
   cmake --build build --target MS2K_Plugin_VST3
   ```
2. Double-click `install-app.bat` and/or `install-vst3.bat`.
3. For the VST3, **re-scan** plug-ins in your DAW afterward
   (Reaper: Options → Preferences → Plug-ins → VST → Re-scan), then route the track's MIDI
   output to your MS2000.

Both binaries are statically linked, so nothing else needs installing.

## Uninstall
- App: delete `%LOCALAPPDATA%\Programs\MS2K Interface` and the two `MS2K Interface.lnk`
  shortcuts (Start Menu + Desktop).
- VST3: delete `%CommonProgramFiles%\VST3\MS2K Editor.vst3`.
