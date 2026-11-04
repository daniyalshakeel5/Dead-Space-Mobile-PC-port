@echo off
setlocal
title Dead Space Mobile - Symbolize Windows crash
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Diagnose-DeadSpace-Crash.ps1"
if errorlevel 1 pause
