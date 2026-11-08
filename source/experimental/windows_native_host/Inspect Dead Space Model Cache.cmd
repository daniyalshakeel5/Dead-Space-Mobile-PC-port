@echo off
setlocal
title Dead Space Mobile - ModelCache crash source report
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Inspect-DeadSpace-ModelCache.ps1"
if errorlevel 1 pause
