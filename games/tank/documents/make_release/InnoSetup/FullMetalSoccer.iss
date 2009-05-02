; -- Example3.iss --
; Same as Example1.iss, but creates some registry entries too.

; SEE THE DOCUMENTATION FOR DETAILS ON CREATING .ISS SCRIPT FILES!

[Setup]
AppId=qc_fms
AppName=Full Metal Soccer
AppVerName=Full Metal Soccer
UsePreviousAppDir=yes
DefaultDirName={pf}\Full Metal Soccer
DefaultGroupName=QuantiCode\Full Metal Soccer
UninstallDisplayIcon={app}\game.ico
OutputDir=.
SetupIconFile=..\SW_Deployment_WIN32_FMS\game.ico
OutputBaseFilename=SetupFMS_v1.0
Compression=lzma
SolidCompression=yes
LicenseFile=..\SW_Deployment_WIN32_FMS\eula.txt
PrivilegesRequired=admin
WizardImageFile=c:\programme\innosetup5\fms_installer.bmp

[Files]
Source: "..\SW_Deployment_WIN32_FMS\*"; DestDir: "{app}"; Permissions: admins-full users-modify authusers-modify everyone-modify;
Source: "..\SW_Deployment_WIN32_FMS\data\*"; DestDir: "{app}\data\"; Flags: recursesubdirs createallsubdirs; Permissions: admins-full users-modify authusers-modify everyone-modify;

[Dirs]
Name: "{app}\screenshots"; Flags: uninsalwaysuninstall; Permissions: admins-full users-modify authusers-modify everyone-modify;


[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";


[Icons]
Name: "{group}\Full Metal Soccer"; Filename: "{app}\autopatcher_client.exe"; WorkingDir: {app}; IconFilename: "{app}\game.ico";
Name: "{userdesktop}\Full Metal Soccer"; Filename: "{app}\autopatcher_client.exe"; WorkingDir: {app}; IconFilename: "{app}\game.ico"; Tasks: desktopicon;
Name: "{group}\Full Metal Soccer Uninstall"; Filename: "{uninstallexe}";
Name: "{group}\Readme"; Filename: "{app}\data\readme.html"; WorkingDir: {app};

[Run]
Filename: "{app}\data\readme.html"; Description: "View the readme"; Flags: postinstall unchecked shellexec skipifsilent skipifdoesntexist


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
            DelTree( ExpandConstant('{userappdata}') + '\QuantiCode\Full Metal Soccer', true, true, true );
            RemoveDir(ExpandConstant('{userappdata}') + '\QuantiCode\Full Metal Soccer');
            RemoveDir(ExpandConstant('{userappdata}') + '\QuantiCode');
          end;

      end;
   end;
end;



