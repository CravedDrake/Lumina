; Inno Setup script. Expects the deployed app folder in %LUMINA_DIST% (default: ..\..\dist\Lumina).
#define AppName "Lumina"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define DistDir GetEnv("LUMINA_DIST")
#if DistDir == ""
  #define DistDir "..\..\dist\Lumina"
#endif

[Setup]
AppId={{6B0E3C58-4F5B-4E55-9B53-7C1D7A0A1A11}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Lumina
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir=..\..\dist
OutputBaseFilename=Lumina-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayIcon={app}\lumina.exe
SetupIconFile=..\..\resources\lumina.ico
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=..\..\LICENSE

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\lumina.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\lumina.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\lumina.exe"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
