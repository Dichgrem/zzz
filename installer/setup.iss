; zzz — Inno Setup Installer
; Build: iscc installer\setup.iss

#define MyAppName "GUI.for.zzz"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "zzz"
#define MyAppURL "https://github.com/diredocks/zzz"
#define MyAppExeName "zzz-gui.exe"

[Setup]
AppId={{E8A1F7D3-6B2C-4F59-9D1E-3C8A7B0F5E2A}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\.output
OutputBaseFilename=zzz-setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\.output\zzz.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\.output\zzz-gui.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "webview2\MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "npcap\npcap-setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\npcap-setup.exe"; Parameters: "/VERYSILENT /NORESTART"; Flags: skipifdoesntexist runhidden; StatusMsg: "安装 Npcap..."; Check: not IsNpcapInstalled
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; Flags: skipifdoesntexist runhidden; StatusMsg: "安装 WebView2..."; Check: not IsWebView2Installed

[Code]
function IsWebView2Installed: Boolean;
begin
  Result := RegKeyExists(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
            RegKeyExists(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}');
end;

function IsNpcapInstalled: Boolean;
begin
  Result := FileExists(ExpandConstant('{sys}\Npcap\wpcap.dll'));
end;

function InitializeSetup: Boolean;
begin
  Result := True;
  if not IsNpcapInstalled then
  begin
    if MsgBox('Npcap is not installed.' + #13#10 + #13#10 +
              'Please install Npcap from https://npcap.com' + #13#10 +
              'before using this application.', mbConfirmation, MB_OK) = IDOK then
    begin
    end;
  end;
end;

function InitializeUninstall: Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  Exec('taskkill.exe', '/f /im zzz-gui.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec('taskkill.exe', '/f /im zzz.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Exec('schtasks.exe', '/delete /tn zzz /f', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if MsgBox('Keep configuration files?' + #13#10#13#10 +
              ExpandConstant('{userappdata}\zzz\'), mbConfirmation, MB_YESNO) = IDNO then
    begin
      Exec('cmd.exe', '/c rmdir /s /q "' + ExpandConstant('{userappdata}\zzz') + '"',
           '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    end;
  end;
end;
