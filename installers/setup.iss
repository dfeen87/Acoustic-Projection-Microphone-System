; APM Headphone Translator - Windows Installer Script
; Compile with Inno Setup 6.0 or later
; https://jrsoftware.org/isinfo.php

#define MyAppName "APM Headphone Translator"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Don Michael Feeney Jr."
#define MyAppURL "https://apm-system.com"
#define MyAppExeName "apm-headphone.exe"

[Setup]
; Basic App Info
AppId={{APM-HEADPHONE-2025}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/support
AppUpdatesURL={#MyAppURL}/updates
AppMutex=APMHeadphoneMutex
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=LICENSE.txt
InfoBeforeFile=README.txt
OutputDir=dist
OutputBaseFilename=APM-Headphone-Setup-{#MyAppVersion}
SetupIconFile=resources\icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64

; Minimum Windows version
MinVersion=10.0.17763

; Required disk space (500MB + language packs)
ExtraDiskSpaceRequired=524288000

; Visual appearance
WizardImageFile=resources\wizard-large.bmp
WizardSmallImageFile=resources\wizard-small.bmp
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=no
DisableReadyPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Types]
Name: "full"; Description: "Full installation (with offline language packs)"
Name: "standard"; Description: "Standard installation (online translation only)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "core"; Description: "Core application"; Types: full standard custom; Flags: fixed
Name: "models"; Description: "Offline Language Packs (650 MB)"; Types: full
Name: "models\en_es"; Description: "English ↔ Spanish"; Types: full; Flags: checkablealone
Name: "models\en_ja"; Description: "English ↔ Japanese"; Types: full; Flags: checkablealone
Name: "models\en_fr"; Description: "English ↔ French"; Types: full; Flags: checkablealone
Name: "models\en_de"; Description: "English ↔ German"; Types: full; Flags: checkablealone
Name: "models\en_zh"; Description: "English ↔ Chinese"; Types: full; Flags: checkablealone
Name: "shortcuts"; Description: "Desktop shortcut"; Types: full standard

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1
Name: "startmenu"; Description: "Add to Start Menu"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startup"; Description: "Launch at Windows startup"; GroupDescription: "Startup options:"; Flags: unchecked

[Files]
; Core application
Source: "build\Release\apm-headphone.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: core
Source: "build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: core
Source: "LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion; Components: core
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme; Components: core
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs; Components: core

; Language models
Source: "models\en-es\*"; DestDir: "{app}\models\en-es"; Flags: ignoreversion recursesubdirs; Components: models\en_es
Source: "models\en-ja\*"; DestDir: "{app}\models\en-ja"; Flags: ignoreversion recursesubdirs; Components: models\en_ja
Source: "models\en-fr\*"; DestDir: "{app}\models\en-fr"; Flags: ignoreversion recursesubdirs; Components: models\en_fr
Source: "models\en-de\*"; DestDir: "{app}\models\en-de"; Flags: ignoreversion recursesubdirs; Components: models\en_de
Source: "models\en-zh\*"; DestDir: "{app}\models\en-zh"; Flags: ignoreversion recursesubdirs; Components: models\en_zh

; Resources
Source: "resources\icons\*"; DestDir: "{app}\resources\icons"; Flags: ignoreversion recursesubdirs; Components: core
Source: "resources\sounds\*"; DestDir: "{app}\resources\sounds"; Flags: ignoreversion recursesubdirs; Components: core

; Visual C++ Runtime (if needed)
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
; Start menu icons
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startmenu
Name: "{group}\User Guide"; Filename: "{app}\docs\INSTALL_USER.md"; Tasks: startmenu
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"; Tasks: startmenu

; Desktop icon
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

; Quick launch icon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

; Startup
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--minimized"; Tasks: startup

[Run]
; Install Visual C++ Runtime
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Installing Visual C++ Runtime..."; Flags: waituntilterminated

; Create initial config
Filename: "{app}\apm-headphone.exe"; Parameters: "--create-config"; StatusMsg: "Creating default configuration..."; Flags: runhidden waituntilterminated

; Launch application
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Cleanup user data (optional)
Filename: "{app}\apm-headphone.exe"; Parameters: "--cleanup"; RunOnceId: "Cleanup"; Flags: runhidden waituntilterminated

[Registry]
; File associations (optional - for saving translation sessions)
Root: HKCR; Subkey: ".apm"; ValueType: string; ValueName: ""; ValueData: "APMSession"; Flags: uninsdeletevalue
Root: HKCR; Subkey: "APMSession"; ValueType: string; ValueName: ""; ValueData: "APM Translation Session"; Flags: uninsdeletekey
Root: HKCR; Subkey: "APMSession\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCR; Subkey: "APMSession\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

; Add to Windows Firewall allowed apps
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Services\SharedAccess\Parameters\FirewallPolicy\FirewallRules"; ValueType: string; ValueName: "APM-Headphone-In"; ValueData: "v2.10|Action=Allow|Active=TRUE|Dir=In|Protocol=6|App={app}\{#MyAppExeName}|Name=APM Headphone Translator|"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Services\SharedAccess\Parameters\FirewallPolicy\FirewallRules"; ValueType: string; ValueName: "APM-Headphone-Out"; ValueData: "v2.10|Action=Allow|Active=TRUE|Dir=Out|Protocol=6|App={app}\{#MyAppExeName}|Name=APM Headphone Translator|"; Flags: uninsdeletevalue

[Code]
var
  AudioTestPage: TInputOptionWizardPage;
  LanguagePage: TInputOptionWizardPage;
  
procedure InitializeWizard;
begin
  // Audio device selection page
  AudioTestPage := CreateInputOptionPage(wpSelectComponents,
    'Audio Setup', 'Test your audio devices',
    'Please test your microphone and headphones to ensure they work correctly.',
    False, False);
  AudioTestPage.Add('I have tested my audio devices');
  AudioTestPage.Add('Skip audio test (not recommended)');
  AudioTestPage.Values[0] := True;
  
  // Primary language selection
  LanguagePage := CreateInputOptionPage(AudioTestPage.ID,
    'Language Selection', 'Choose your primary languages',
    'Select the languages you will use most frequently.',
    False, False);
  LanguagePage.Add('English → Spanish');
  LanguagePage.Add('English → Japanese');
  LanguagePage.Add('English → French');
  LanguagePage.Add('English → German');
  LanguagePage.Add('English → Chinese');
  LanguagePage.Values[0] := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  
  // Audio test reminder
  if CurPageID = AudioTestPage.ID then
  begin
    if AudioTestPage.Values[1] then
    begin
      if MsgBox('You have chosen to skip the audio test. The application may not work correctly if your audio devices are not properly configured.' + #13#10#13#10 + 'Continue anyway?', mbConfirmation, MB_YESNO) = IDNO then
        Result := False;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    // Create user config directory
    CreateDir(ExpandConstant('{userappdata}\APMHeadphone'));
    
    // Save language preferences
    SaveStringToFile(ExpandConstant('{userappdata}\APMHeadphone\first-run.txt'), 
      'language_preference=' + IntToStr(LanguagePage.SelectedValueIndex) + #13#10, False);
  end;
end;

function InitializeUninstall(): Boolean;
begin
  Result := True;
  if MsgBox('Do you want to keep your settings and downloaded language packs?', mbConfirmation, MB_YESNO) = IDYES then
  begin
    // Keep user data
    RegWriteStringValue(HKCU, 'Software\APMHeadphone', 'KeepData', 'true');
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  KeepData: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if RegQueryStringValue(HKCU, 'Software\APMHeadphone', 'KeepData', KeepData) and (KeepData = 'true') then
    begin
      // Keep user data
      MsgBox('Your settings and language packs have been preserved in:' + #13#10 + 
             ExpandConstant('{userappdata}\APMHeadphone'), mbInformation, MB_OK);
    end
    else
    begin
      // Remove all user data
      DelTree(ExpandConstant('{userappdata}\APMHeadphone'), True, True, True);
    end;
    
    // Clean up registry
    RegDeleteKeyIncludingSubkeys(HKCU, 'Software\APMHeadphone');
  end;
end;

function GetUninstallString: String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  sUnInstPath := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#emit SetupSetting("AppId")}_is1');
  sUnInstallString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString);
  Result := sUnInstallString;
end;

function IsUpgrade: Boolean;
begin
  Result := (GetUninstallString <> '');
end;

function UnInstallOldVersion: Integer;
var
  sUnInstallString: String;
  iResultCode: Integer;
begin
  Result := 0;
  sUnInstallString := GetUninstallString;
  if sUnInstallString <> '' then begin
    sUnInstallString := RemoveQuotes(sUnInstallString);
    if Exec(sUnInstallString, '/SILENT /NORESTART /SUPPRESSMSGBOXES','', SW_HIDE, ewWaitUntilTerminated, iResultCode) then
      Result := 3
    else
      Result := 2;
  end else
    Result := 1;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if IsUpgrade then
  begin
    UnInstallOldVersion;
  end;
end;

[Messages]
WelcomeLabel1=Welcome to the [name] Setup Wizard
WelcomeLabel2=This will install [name/ver] on your computer.%n%nTransform your headphones into a real-time translation device! Break language barriers with cutting-edge audio processing and AI-powered translation.%n%nIt is recommended that you close all other applications before continuing.
FinishedLabel=Setup has finished installing [name] on your computer.%n%nYour headphones are now ready for real-time translation!

[CustomMessages]
LaunchProgram=Launch %1 now
