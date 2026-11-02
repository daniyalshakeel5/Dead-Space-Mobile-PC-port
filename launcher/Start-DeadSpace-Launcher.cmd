@echo off
setlocal
cd /d "%~dp0"
start "" /B powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0Launcher.ps1"
exit /b 0
