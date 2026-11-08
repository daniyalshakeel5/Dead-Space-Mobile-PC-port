@echo off
setlocal
title Dead Space Mobile - Diagnose Existing Linux Build
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Diagnose-WSL-Crash.ps1" %*
if errorlevel 1 pause
