@echo off
set STARTUP_DIR=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
del "%STARTUP_DIR%\MyPet.lnk"
echo 已取消开机自启。
pause