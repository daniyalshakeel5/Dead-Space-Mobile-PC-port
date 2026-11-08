@echo off
setlocal
title Dead Space Mobile - build native Windows game
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-DeadSpace-Windows.ps1"
if errorlevel 1 (
 echo Build did not finish. Your original game is unchanged.
 pause
)
