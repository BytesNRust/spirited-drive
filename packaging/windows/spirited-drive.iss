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
; always ask for the folder, the previous one (UsePreviousAppDir, on by default) as the default: an update must not go silently into a folder the player forgot
DisableDirPage=no
UsePreviousAppDir=yes
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
; the program was called tnfs.exe before: a folder where an older zip was unpacked by hand keeps it beside the new Spirited Drive.exe, and it would start the old version
Type: files; Name: "{app}\tnfs.exe"
Type: files; Name: "{app}\Spirited Drive.exe"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Spirited Drive"; Filename: "{app}\Spirited Drive.exe"
Name: "{autodesktop}\Spirited Drive"; Filename: "{app}\Spirited Drive.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Spirited Drive.exe"; Description: "Start Spirited Drive"; Flags: nowait postinstall skipifsilent

[Code]
{ Browse appends the default folder name to the folder picked (AppendDefaultDirName): picking an existing "Spirited Drive" folder gave ...\Spirited Drive\Spirited Drive, one level
  below it. A path that ends in two of them is the one level the player meant. }
function CollapsedDir(const Dir: String): String;
var
  Parent: String;
begin
  Result := Dir;
  Parent := ExtractFileDir(Dir);
  if (CompareText(ExtractFileName(Dir), 'Spirited Drive') = 0) and (CompareText(ExtractFileName(Parent), 'Spirited Drive') = 0) then Result := Parent;
end;

{ an update: an earlier install is registered (the previous folder) and its program is still there }
function IsUpdate: Boolean;
begin
  Result := (WizardForm.PrevAppDir <> '') and FileExists(AddBackslash(WizardForm.PrevAppDir) + 'Spirited Drive.exe');
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectDir then begin
    if IsUpdate then begin
      { the folder is shown, not asked: the game is updated where it is }
      WizardForm.DirEdit.Text := RemoveBackslashUnlessRoot(WizardForm.PrevAppDir);
      WizardForm.DirEdit.Enabled := False;
      WizardForm.DirBrowseButton.Visible := False;
      WizardForm.PageNameLabel.Caption := 'Update Spirited Drive';
      WizardForm.PageDescriptionLabel.Caption := 'Spirited Drive is already installed on this computer.';
      WizardForm.SelectDirLabel.Caption := 'Spirited Drive is already installed in the folder below and will be updated there. Your settings, records and the copy of the original game are kept.';
      WizardForm.SelectDirBrowseLabel.Caption := 'Click Next to continue.';
    end else if WizardForm.PrevAppDir <> '' then
      { an earlier install whose program is gone: not its folder, the usual one }
      WizardForm.DirEdit.Text := ExpandConstant('{localappdata}\Programs\Spirited Drive');
  end;
  if (CurPageID = wpReady) and IsUpdate then WizardForm.NextButton.Caption := 'Update';
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if (CurPageID = wpSelectDir) and not IsUpdate then WizardForm.DirEdit.Text := CollapsedDir(RemoveBackslashUnlessRoot(WizardDirValue));
  Result := True;
end;

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
