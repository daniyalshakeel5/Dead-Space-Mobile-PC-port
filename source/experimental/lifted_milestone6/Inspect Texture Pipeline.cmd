@echo off
setlocal
title Dead Space Mobile - Inspect Texture Pipeline Data Flow
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Inspect-TexturePipeline.ps1" %*
if errorlevel 1 pause
