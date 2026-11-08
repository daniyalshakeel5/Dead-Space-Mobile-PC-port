@echo off
setlocal
title Dead Space Mobile - Install milestone 6 and launch WSL
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Milestone6.ps1"
if errorlevel 1 (
  echo Update did not complete. Nothing was deleted.
  pause
)
