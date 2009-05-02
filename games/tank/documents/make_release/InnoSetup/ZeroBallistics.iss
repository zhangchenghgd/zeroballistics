; -- Example3.iss --
; Same as Example1.iss, but creates some registry entries too.

; SEE THE DOCUMENTATION FOR DETAILS ON CREATING .ISS SCRIPT FILES!

[Setup]
AppId=qc_zb_1.1.1
AppName=Zero Ballistics
AppVerName=Zero Ballistics 1.1.1
DefaultDirName={pf}\Zero Ballistics
DefaultGroupName=QuantiCode\Zero Ballistics
UninstallDisplayIcon={app}\game.ico
OutputDir=.
SetupIconFile=..\SW_Deployment_WIN32\game.ico
OutputBaseFilename=SetupZB_v1.1.1
Compression=lzma
SolidCompression=yes

[Files]
Source: "..\SW_Deployment_WIN32\*"; DestDir: "{app}";
Source: "..\SW_Deployment_WIN32\data\*"; DestDir: "{app}\data\"; Flags: recursesubdirs createallsubdirs
;Source: "..\SW_Deployment_WIN32\screenshots"; DestDir: "{app}\screenshots\"; Flags: recursesubdirs createallsubdirs

[Dirs]
Name: "{app}\screenshots"; Flags: uninsalwaysuninstall


[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";


[Icons]
Name: "{group}\Zero Ballistics"; Filename: "{app}\tankClient.exe"; WorkingDir: {app}; IconFilename: "{app}\game.ico";
Name: "{userdesktop}\Zero Ballistics"; Filename: "{app}\tankClient.exe"; WorkingDir: {app}; IconFilename: "{app}\game.ico"; Tasks: desktopicon;
Name: "{group}\Zero Ballistics Uninstall"; Filename: "{uninstallexe}";
Name: "{group}\Readme"; Filename: "{app}\readme.html"; WorkingDir: {app};

[Run]
Filename: "{app}\readme.html"; Description: "View the readme"; Flags: postinstall unchecked shellexec skipifsilent skipifdoesntexist


; NOTE: Most apps do not need registry entries to be pre-created. If you
; don't know what the registry is or if you need to use it, then chances are
; you don't need a [Registry] section.

[Registry]
; Start "Software\My Company\My Program" keys under HKEY_CURRENT_USER
; and HKEY_LOCAL_MACHINE. The flags tell it to always delete the
; "My Program" keys upon uninstall, and delete the "My Company" keys
; if there is nothing left in them.
;Root: HKCU; Subkey: "Software\My Company"; Flags: uninsdeletekeyifempty
;Root: HKCU; Subkey: "Software\My Company\My Program"; Flags: uninsdeletekey
;Root: HKLM; Subkey: "Software\My Company"; Flags: uninsdeletekeyifempty
;Root: HKLM; Subkey: "Software\My Company\My Program"; Flags: uninsdeletekey
;Root: HKLM; Subkey: "Software\My Company\My Program\Settings"; ValueType: string; ValueName: "Path"; ValueData: "{app}"



[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  user_answer: Integer;
begin
  case CurUninstallStep of
    usUninstall:
      begin
        user_answer := MsgBox('Do you want to delete the local settings?', mbConfirmation, MB_YESNO);

        if user_answer = IDYES then
          begin
            DelTree( ExpandConstant('{userappdata}') + '\QuantiCode\Zero Ballistics', true, true, true );
            RemoveDir(ExpandConstant('{userappdata}') + '\QuantiCode\Zero Ballistics');
            RemoveDir(ExpandConstant('{userappdata}') + '\QuantiCode');
          end;

      end;
   end;
end;



