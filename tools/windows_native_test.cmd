@echo off
setlocal
cd /d "%~dp0"
title Dead Space Mobile - Native Windows Field Test
echo Dead Space Mobile native Windows test launcher
echo Working folder: %CD%
echo.
if not exist "deadspace_native.exe" (
    echo ERROR: deadspace_native.exe not found next to this launcher.
    goto noexe
)
if exist "installed-deadspace\assets\published\models\main_menu.m3g" goto installed
if exist "Deas.Space.Mobile.Fixes.apk" goto import
echo ERROR: Original game APK not found.
echo Copy your own Deas.Space.Mobile.Fixes.apk beside the executable.
echo Do NOT upload the APK or original assets to GitHub.
goto missingapk

:installed
echo Found installed original assets. Starting without re-extraction...
deadspace_native.exe --boot-splash --assets-dir ".\installed-deadspace" > ".\deadspace-test.log" 2>&1
goto finished

:import
echo Installing assets from your original APK and starting native splash/menu...
deadspace_native.exe --boot-apk --apk ".\Deas.Space.Mobile.Fixes.apk" --assets-dir ".\installed-deadspace" > ".\deadspace-test.log" 2>&1
goto finished

:finished
set "DS_EXIT=%ERRORLEVEL%"
echo.
echo ---------------- Native diagnostic output ----------------
if exist ".\deadspace-test.log" type ".\deadspace-test.log"
echo ----------------------------------------------------------
echo.
echo Native process exit code: %DS_EXIT%
if "%DS_EXIT%"=="20" echo NOTE: Exit 20 means gameplay is not reconstructed yet. This is not a Windows loader crash.
if "%DS_EXIT%"=="4" echo NOTE: Exit 4 means private asset installation failed. Choose an empty extraction directory if the existing one is incomplete.
if "%DS_EXIT%"=="7" echo NOTE: Exit 7 means original splash assets are missing from the installed directory.
if "%DS_EXIT%"=="8" echo NOTE: Exit 8 means original menu/splash resource decoding failed.
if "%DS_EXIT%"=="9" echo NOTE: Exit 9 means the native SDL2 splash window could not be created.
echo.
echo If the program failed, share deadspace-test.log and the exact
echo Windows dialog text or screenshot. Do not upload copyrighted game assets.
goto done

:noexe
set "DS_EXIT=2"
goto done
:missingapk
set "DS_EXIT=3"
:done
echo.
pause
exit /b %DS_EXIT%
