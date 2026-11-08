@echo off
setlocal
title Dead Space Mobile - Repair Windows build
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Repair-DeadSpace-Windows.ps1"
if errorlevel 1 (
 echo.
 echo Repair failed. See Documents\DeadSpaceMobilePC\repair.log and windows-build\compile-windows.log
 pause
)
