; Inno Setup script — MS2K Interface standalone editor.
; Produces a single self-contained installer .exe that bundles the (statically
; linked) app, installs it, and creates Start Menu / optional Desktop shortcuts.
; Build:  "ISCC.exe" installers\app.iss   (output -> dist\)

#define AppName "MS2K Interface"
#define AppVersion "1.1.0"
#define AppPublisher "Lou Silvestri"
#define AppURL "https://github.com/louissilvestri/MS2K_Interface"
#define AppExe "MS2K_Interface.exe"

[Setup]
AppId={{A1F4C7E2-9B3D-4E6A-8C12-5D7F0B9E2A41}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
SetupIconFile=..\resources\icon.ico
UninstallDisplayIcon={app}\{#AppExe}
; Per-user by default (no UAC); the user may choose all-users on the wizard.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\dist
OutputBaseFilename=MS2K_Interface-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "..\build\MS2K_Interface_artefacts\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName} now"; Flags: nowait postinstall skipifsilent
