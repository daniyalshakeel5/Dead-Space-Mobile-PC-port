@echo off
setlocal
title Dead Space Mobile - Controller Test
powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Launch.ps1"
if errorlevel 1 pause
