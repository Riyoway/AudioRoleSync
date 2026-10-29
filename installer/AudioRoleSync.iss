#define MyAppName "AudioRoleSync"
#define MyAppVersion "0.0.1"
#define MyAppExeName "AudioRoleSync.exe"

[Setup]
AppId={{B4D2E97F-4F8F-4C29-9A20-204E7811B23F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=AudioRoleSync-v{#MyAppVersion}-win-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\resources\app.ico
WizardStyle=modern
CloseApplications=yes

[Files]
Source: "..\dist\win-x64\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "AudioRoleSync"; ValueData: """{app}\{#MyAppExeName}"" --startup"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\AudioRoleSync"; ValueType: dword; ValueName: "StartupConfigured"; ValueData: "1"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Start AudioRoleSync"; Flags: nowait
