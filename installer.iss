; Script Inno Setup tạo bộ cài đặt Themis x64 cho Windows
; Cài đặt chuẩn xác vào Program Files (64-bit, không có x86)

#define MyAppName "Themis"
#define MyAppVersion "1.9.0"
#define MyAppPublisher "Lê Minh Hoàng & Đỗ Đức Đông"
#define MyAppURL "https://github.com/ubuntu2310fake/Themis-x64-unofficial"
#define MyAppExeName "ThemisLinux.exe"

[Setup]
; AppId định danh duy nhất cho Themis
AppId={{105A3ABE-8D8D-44CD-9E9E-14A872C93D41}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion} (x64)
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Cài đặt vào Program Files (x64), hoàn toàn không có (x86)
DefaultDirName={commonpf64}\Themis
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=.
OutputBaseFilename=Themis-Setup-x64
SetupIconFile=themis.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

; Bắt buộc chế độ cài đặt 64-bit thuần túy
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "contestassociation"; Description: "Tự động mở các tệp kỳ thi (*.contest) bằng Themis"; GroupDescription: "Liên kết tệp:"

[Files]
Source: "Themis-Windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Liên kết định dạng file .contest
Root: HKA; Subkey: "Software\Classes\.contest"; ValueType: string; ValueName: ""; ValueData: "ThemisContestFile"; Flags: uninsdeletevalue; Tasks: contestassociation
Root: HKA; Subkey: "Software\Classes\ThemisContestFile"; ValueType: string; ValueName: ""; ValueData: "Tệp kỳ thi Themis (.contest)"; Flags: uninsdeletekey; Tasks: contestassociation
Root: HKA; Subkey: "Software\Classes\ThemisContestFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: contestassociation
Root: HKA; Subkey: "Software\Classes\ThemisContestFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: contestassociation

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
