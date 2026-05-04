@echo off
:: 获取当前脚本所在的目录
set CURRENT_DIR=%~dp0

:: 定义目标程序路径
set TARGET_EXE=%CURRENT_DIR%pet.exe

:: 定义启动文件夹路径
set STARTUP_DIR=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup

:: 使用 PowerShell 命令来创建快捷方式（因为纯批处理创建快捷方式很麻烦，调用 PowerShell 更稳定）
powershell -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut('%STARTUP_DIR%\MyPet.lnk'); $s.TargetPath='%TARGET_EXE%'; $s.WorkingDirectory='%CURRENT_DIR%'; $s.Save()"

echo 部署完成！程序已加入开机自启。
pause