@echo off
setlocal
title Dead Space Mobile PC - WSL Launcher
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0DeadSpaceMobile-WSL.ps1" %*
set "result=%ERRORLEVEL%"
if not "%result%"=="0" (
 echo.
 echo Launcher failed with exit code %result%.
 echo Please copy the error text above when reporting the problem.
 pause
)
exit /b %result%
