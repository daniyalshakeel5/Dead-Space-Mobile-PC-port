@echo off
setlocal
title Dead Space Mobile - Fresh Windows Install
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-DeadSpace-Fresh.ps1"
if errorlevel 1 (
  echo The fresh installation did not complete. See the message above.
  pause
)
