; Inno Setup 安装脚本 —— 鲨鲨桌宠
; 下载 Inno Setup：https://jrsoftware.org/isinfo.php
; 编译：用 Inno Setup Compiler 打开此文件，点击 Build → Compile

#define AppName      "鲨鲨桌宠"
#define AppNameEn    "shasha"
#define AppVersion   "1.0.0"
#define AppPublisher "jcfszxc"
#define AppExeName   "shasha.exe"
#define SourceDir    "dist\shasha"        ; PyInstaller 输出目录

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisherURL=https://github.com
AppSupportURL=https://github.com
AppUpdatesURL=https://github.com
DefaultDirName={autopf}\{#AppNameEn}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=installer
OutputBaseFilename=shasha_setup_{#AppVersion}
SetupIconFile=data\tray.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; 不需要管理员权限，安装到用户目录
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create desktop shortcut"; GroupDescription: "Additional tasks:"
Name: "autostart";   Description: "Auto-start on Windows login"; GroupDescription: "Additional tasks:"; Flags: unchecked

[Files]
; 复制整个 PyInstaller 输出目录
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; 开始菜单
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExeName}"
Name: "{group}\卸载 {#AppName}";      Filename: "{uninstallexe}"
; 桌面快捷方式（可选）
Name: "{autodesktop}\{#AppName}";     Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
; 开机自启动（仅当用户勾选时写入）
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
  ValueType: string; ValueName: "shasha_pet"; \
  ValueData: """{app}\{#AppExeName}"""; \
  Flags: uninsdeletevalue; Tasks: autostart

[Run]
; 安装完成后可选择立即启动
Filename: "{app}\{#AppExeName}"; \
  Description: "立即启动鲨鲨桌宠"; \
  Flags: nowait postinstall skipifsilent

[UninstallRun]
; 卸载前先关闭进程
Filename: "taskkill.exe"; Parameters: "/f /im {#AppExeName}"; Flags: runhidden

[UninstallDelete]
; 卸载时清理用户数据（可选，注释掉则保留存档）
; Type: filesandordirs; Name: "{app}\data\shasha\petconfig.ini"