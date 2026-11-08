@echo off
setlocal
title Dead Space Mobile - Diagnose Linux TexturePack
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Diagnose-TexturePack-Linux.ps1" %*
if errorlevel 1 pause
