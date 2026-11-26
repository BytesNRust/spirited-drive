; Spirited Drive's Windows installer (Inno Setup 6): the same folder as the zip, installed for the current user, no administrator.
; Built by tools/package_windows_installer.sh from the unpacked zip:
;   ISCC /DSourceDir=<folder holding "Spirited Drive.exe"> /DAppVersion=<version> /DVersionNumeric=<a.b.c.0> /DOutputDir=<dir> /DOutputName=<file name without .exe> spirited-drive.iss
; The uninstaller removes what this installer put in {app} and nothing else. The player's own folder (%APPDATA%\Spirited Drive: settings, the copy of the CD,
; mods, logs) is not in {app} and no line below deletes anything there: tools/installer_smoke.sh checks it with a marker file.

#ifndef SourceDir
  #error SourceDir is not defined: the folder that holds "Spirited Drive.exe" and the data folder
#endif
#ifndef AppVersion
  #error AppVersion is not defined
#endif
#ifndef VersionNumeric
  #define VersionNumeric "0.0.0.0"
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef OutputName
  #define OutputName "Spirited-Drive-setup"
#endif

[Setup]
; never changes: it makes Windows (and the installer) see a new installer over an old one as an update of the same program
AppId={{920C49EB-3A70-4CEF-85E8-1C6EC5005CB2}
AppName=Spirited Drive
AppVersion={#AppVersion}
AppVerName=Spirited Drive {#AppVersion}
AppPublisher=Bytes n Rust
AppPublisherURL=https://github.com/BytesNRust/spirited-drive
AppSupportURL=https://github.com/BytesNRust/spirited-drive/issues
VersionInfoVersion={#VersionNumeric}
DefaultDirName={localappdata}\Programs\Spirited Drive
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Spirited Drive.exe
UninstallDisplayName=Spirited Drive
; a running game is asked to close (Windows' Restart Manager) so its exe can be replaced
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[InstallDelete]
; an update over an older version: what the program carried before and the new version may not carry any more goes first. Only the program's own files and
; folders: nothing else in {app}, nothing outside it.
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\LICENSES"
Type: files; Name: "{app}\*.dll"
Type: files; Name: "{app}\*.txt"
Type: files; Name: "{app}\*.md"
Type: files; Name: "{app}\LICENSE"
Type: files; Name: "{app}\Spirited Drive.exe"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Spirited Drive"; Filename: "{app}\Spirited Drive.exe"
Name: "{autodesktop}\Spirited Drive"; Filename: "{app}\Spirited Drive.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Spirited Drive.exe"; Description: "Start Spirited Drive"; Flags: nowait postinstall skipifsilent

[Code]
{ the player's folder is kept: say so, once, where it is (not in a silent uninstall) }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Text: String;
begin
  if (CurUninstallStep = usDone) and not UninstallSilent then begin
    { a line of the script may not begin with a hash sign: the preprocessor takes it for a directive }
    Text := 'Your settings, the copy of the original game and your mods were kept in:' + #13#10 + ExpandConstant('{userappdata}\Spirited Drive');
    Text := Text + #13#10 + #13#10 + 'Delete that folder yourself if you want them gone.';
    MsgBox(Text, mbInformation, MB_OK);
  end;
end;
