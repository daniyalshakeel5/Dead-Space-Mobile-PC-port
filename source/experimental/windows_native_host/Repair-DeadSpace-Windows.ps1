[CmdletBinding()]
param([string]$InstallDir = "", [switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
$transcript = $false
try {
    if (!$InstallDir) {
        $docs = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
        if (!$docs) { $docs = Join-Path $env:USERPROFILE 'Documents' }
        $InstallDir = Join-Path $docs 'DeadSpaceMobilePC'
    }
    $InstallDir = [IO.Path]::GetFullPath($InstallDir)
    $toolsDir = Join-Path $InstallDir 'tools'
    foreach ($required in @(
        'apk\lib\armeabi\libDeadSpace.so',
        'apk\assets',
        'tools\out\protos.h',
        'tools\out\tables.c')) {
        if (!(Test-Path -LiteralPath (Join-Path $InstallDir $required))) {
            throw "Existing private game install is missing $required. Do not delete the original installation."
        }
    }
    $packageTools = Join-Path $PSScriptRoot 'tools'
    $patchFiles = @(
        'build_win32_lifted.sh',
        'modelcache_empty_vector.py',
        'object3d_duplicate_probe.py',
        'win32_import_bridge.h',
        'win32_missing_imports.c',
        'win32_missing_gles.c',
        'jni_vm.c',
        'libc_files.c',
        'win32_bionic_openflags.h',
        'windows_lifted_compat.h'
    )
    foreach ($name in $patchFiles) {
        if (!(Test-Path -LiteralPath (Join-Path $packageTools $name))) {
            throw "This repair kit is incomplete. Missing tools\$name."
        }
    }
    $repairLog = Join-Path $InstallDir 'repair.log'
    Start-Transcript -LiteralPath $repairLog -Append | Out-Null
    $transcript = $true
    Write-Host "[DS REPAIR] Preserving original APK, translated C and compiled objects."
    foreach ($name in $patchFiles) {
        Copy-Item -LiteralPath (Join-Path $packageTools $name) -Destination (Join-Path $toolsDir $name) -Force
    }
    if (!(Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        throw 'WSL is required as the Windows x86 cross-compiler.'
    }
    $wslGame = & wsl.exe --exec wslpath -a -u $InstallDir
    if ($LASTEXITCODE -ne 0 -or !$wslGame) {
        throw 'Could not translate your existing installation path to WSL.'
    }
    $wslTools = & wsl.exe --exec wslpath -a -u $toolsDir
    if ($LASTEXITCODE -ne 0 -or !$wslTools) {
        throw 'Could not translate the tools path to WSL.'
    }
    $wslGame = ($wslGame | Select-Object -First 1).Trim()
    $wslTools = ($wslTools | Select-Object -First 1).Trim()
    Write-Host '[DS REPAIR] Re-linking the 32-bit Windows native executable...'
    & wsl.exe --exec bash "$wslTools/build_win32_lifted.sh" "$wslGame"
    if ($LASTEXITCODE -ne 0) {
        throw "Build still failed; send $InstallDir\windows-build\compile-windows.log."
    }
    $work = Join-Path $InstallDir 'windows-build'
    $exe = Join-Path $work 'DeadSpaceMobile-Win32.exe'
    if (!(Test-Path -LiteralPath $exe)) { throw 'Compiler reported success but the Windows exe is missing.' }
    $launch = Join-Path $work 'Play Dead Space Windows.cmd'
    $launchText = @'
@echo off
setlocal
title Dead Space Mobile - Native Windows
cd /d "%~dp0"
if not exist logs mkdir logs
if not exist "%~dp0..\apk\assets\var" mkdir "%~dp0..\apk\assets\var"
if not exist "%~dp0..\apk\assets\var" echo [DS SAVE ERROR] Cannot create app var directory.
set "DS_WINDOW=1"
set "DS_STEPS=create,surface,frame"
set "DS_FRAMES=0"
set "DS_ASSETS=%~dp0..\apk\assets"
set "DS_DATA=%~dp0..\apk\assets"
set "DS_MODEL=R800i"
set "DS_MANUFACTURER=Sony Ericsson"
"DeadSpaceMobile-Win32.exe" "%~dp0..\apk\lib\armeabi\libDeadSpace.so" > "logs\windows-run.log" 2>&1
set "rc=%errorlevel%"
echo Game exit code: %rc%
if not "%rc%"=="0" (
 echo Please send windows-build\logs\windows-run.log and crash-symbolized.log
 if exist "%~dp0Diagnose-DeadSpace-Crash.ps1" powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Diagnose-DeadSpace-Crash.ps1" -InstallDir "%~dp0.." -Quiet
 if exist "%~dp0logs\crash-symbolized.log" (
   start "" notepad "%~dp0logs\crash-symbolized.log"
 ) else (
   start "" notepad "%~dp0logs\windows-run.log"
 )
 pause
)
'@
    Set-Content -LiteralPath $launch -Value $launchText -Encoding Ascii
    $diagnoser = Join-Path $PSScriptRoot 'Diagnose-DeadSpace-Crash.ps1'
    if (Test-Path -LiteralPath $diagnoser) {
        Copy-Item -LiteralPath $diagnoser -Destination (Join-Path $work 'Diagnose-DeadSpace-Crash.ps1') -Force
    }
    Write-Host "[DS REPAIR] Native PE32 executable ready: $exe"
    Write-Host '[DS REPAIR] This confirms linking, not gameplay or startup success.'
    if ($transcript) { Stop-Transcript | Out-Null; $transcript = $false }
    if (!$NoLaunch) { Start-Process -FilePath $launch -WorkingDirectory $work }
    exit 0
} catch {
    Write-Host "[DS REPAIR ERROR] $($_.Exception.Message)" -ForegroundColor Red
    if ($transcript) { Stop-Transcript | Out-Null }
    exit 1
}
