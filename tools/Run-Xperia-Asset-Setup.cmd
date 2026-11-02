@echo off
setlocal
cd /d "%~dp0\.."
title Dead Space Mobile - Xperia and Reanimated Asset Setup
echo This installer asks for two user-owned game files:
echo  1. Xperia Play deadspace.zip (required)
echo  2. DeadSpaceReanimated APK (optional HD texture layer)
echo.
where py >nul 2>&1
if not errorlevel 1 (
  py -3 -m tools.xperia_vita_donor_install --gui
) else (
  where python >nul 2>&1
  if errorlevel 1 (
    echo ERROR: Python 3 was not found. Install Python 3 with tkinter.
    goto end
  )
  python -m tools.xperia_vita_donor_install --gui
)
echo.
echo This installs files only. The ARM game is not yet runnable on x86-64.
:end
pause
endlocal
