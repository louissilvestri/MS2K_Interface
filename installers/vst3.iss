; Inno Setup script — MS2K Editor VST3 plugin.
; Produces a single self-contained installer .exe that bundles the .vst3 and
; installs it into the system VST3 folder DAWs scan. Requires admin (the wizard
; elevates). Build:  "ISCC.exe" installers\vst3.iss   (output -> dist\)

#define AppName "MS2K Editor (VST3)"
#define AppVersion "1.0.0"
#define AppPublisher "Lou Silvestri"
#define AppURL "https://github.com/louissilvestri/MS2K_Interface"
#define Bundle "MS2K Editor.vst3"

[Setup]
AppId={{C3D8E5F1-2A4B-4C6D-9E10-7F3B5A8C1D92}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
; VST3 must live in the common VST3 folder, so fix the location and require admin.
DefaultDirName={commoncf}\VST3\{#Bundle}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
UninstallDisplayName={#AppName}
OutputDir=..\dist
OutputBaseFilename=MS2K-Editor-VST3-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "..\build\MS2K_Plugin_artefacts\VST3\{#Bundle}\*"; DestDir: "{commoncf}\VST3\{#Bundle}"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedLabel=Setup installed the {#AppName} into your system VST3 folder.%n%nIn your DAW, re-scan VST plug-ins (Reaper: Options > Preferences > Plug-ins > VST > Re-scan), then route the track's MIDI output to your MS2000.
