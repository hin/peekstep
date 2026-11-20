; Peekstep installer (Inno Setup 6). Installs for the current user only,
; without admin rights. Build with:
;   iscc /DAppVersion=0.1.0 /DSourceDir=<folder with peekstep.exe> /DOutputDir=<out> peekstep.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "."
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{6C1F4E4B-3E0B-4F67-9C55-5A1C2B7E9D10}
AppName=Peekstep
AppVersion={#AppVersion}
AppVerName=Peekstep {#AppVersion}
AppPublisherURL=https://github.com/hin/peekstep
AppSupportURL=https://github.com/hin/peekstep/issues
DefaultDirName={autopf}\Peekstep
DefaultGroupName=Peekstep
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
UninstallDisplayIcon={app}\peekstep.exe
LicenseFile={#SourceDir}\LICENSE.txt
OutputDir={#OutputDir}
OutputBaseFilename=peekstep-{#AppVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "assoc"; Description: "Open .step and .stp files with Peekstep (double-click in Explorer)"; GroupDescription: "File types:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\peekstep.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD-PARTY-LICENSES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Peekstep"; Filename: "{app}\peekstep.exe"
Name: "{autodesktop}\Peekstep"; Filename: "{app}\peekstep.exe"; Tasks: desktopicon

[Registry]
; Always: list Peekstep under "Open with" for STEP files.
Root: HKCU; Subkey: "Software\Classes\Applications\peekstep.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "Peekstep"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\peekstep.exe\SupportedTypes"; ValueType: string; ValueName: ".step"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\peekstep.exe\SupportedTypes"; ValueType: string; ValueName: ".stp"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\Applications\peekstep.exe\shell\open\command"; ValueType: string; ValueData: """{app}\peekstep.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\Peekstep.step"; ValueType: string; ValueData: "STEP 3D model"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Peekstep.step\DefaultIcon"; ValueType: string; ValueData: "{app}\peekstep.exe,0"
Root: HKCU; Subkey: "Software\Classes\Peekstep.step\shell\open\command"; ValueType: string; ValueData: """{app}\peekstep.exe"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\.step\OpenWithProgids"; ValueType: string; ValueName: "Peekstep.step"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.stp\OpenWithProgids"; ValueType: string; ValueName: "Peekstep.step"; ValueData: ""; Flags: uninsdeletevalue
; Optional: make Peekstep the double-click app. Windows still lets the user
; pick something else later, and keeps an earlier explicit choice.
Root: HKCU; Subkey: "Software\Classes\.step"; ValueType: string; ValueData: "Peekstep.step"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCU; Subkey: "Software\Classes\.stp"; ValueType: string; ValueData: "Peekstep.step"; Flags: uninsdeletevalue; Tasks: assoc

[Run]
Filename: "{app}\peekstep.exe"; Description: "Start Peekstep"; Flags: nowait postinstall skipifsilent
