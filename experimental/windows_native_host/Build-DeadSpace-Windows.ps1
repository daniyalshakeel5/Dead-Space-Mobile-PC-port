[CmdletBinding()]
param([string]$ProjectDir="")
$ErrorActionPreference="Stop"

function ValidProject([string]$folder) {
    if (!$folder -or !(Test-Path -LiteralPath $folder -PathType Container)) { return $false }
    return (Test-Path -LiteralPath (Join-Path $folder "tools\out\protos.h")) -and
           (Test-Path -LiteralPath (Join-Path $folder "apk\lib\armeabi\libDeadSpace.so")) -and
           (Test-Path -LiteralPath (Join-Path $folder "apk\assets") -PathType Container)
}
function PickProject {
    $downloads=Join-Path $env:USERPROFILE "Downloads"
    $known=Join-Path $downloads "dspc_milestone5_one_click_vscode\dspc"
    if (ValidProject $known) { return $known }
    foreach($dir in (Get-ChildItem -LiteralPath $downloads -Directory -ErrorAction SilentlyContinue)) {
        if (ValidProject $dir.FullName) { return $dir.FullName }
        $child=Join-Path $dir.FullName "dspc"
        if (ValidProject $child) { return $child }
    }
    Add-Type -AssemblyName System.Windows.Forms
    $choose=New-Object System.Windows.Forms.FolderBrowserDialog
    $choose.Description="Select the dspc project folder containing tools\out and apk\assets"
    $choose.ShowNewFolderButton=$false
    if ($choose.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        throw "No project chosen. No existing game files were changed."
    }
    return $choose.SelectedPath
}
function Inform([string]$msg,[bool]$bad) {
    Add-Type -AssemblyName System.Windows.Forms
    $ico=if($bad){[System.Windows.Forms.MessageBoxIcon]::Error}else{[System.Windows.Forms.MessageBoxIcon]::Information}
    [void][System.Windows.Forms.MessageBox]::Show($msg,"Dead Space Mobile Windows build",[System.Windows.Forms.MessageBoxButtons]::OK,$ico)
}
try {
    $package=Split-Path -Parent $MyInvocation.MyCommand.Path
    $src=Join-Path $package "tools"
    $required=@("host.c","jni_vm.c","threads.c","libc_files.c","gl_shim.c",
                "front_sdl.c","gen_gl.py","windows_lifted_compat.h",
                "win32_substrate.c","win32_substrate.h","build_win32_lifted.sh")
    foreach($name in $required) {
        if (!(Test-Path -LiteralPath (Join-Path $src $name))) { throw "Incomplete update: missing $name" }
    }
    if (!$ProjectDir) { $ProjectDir=PickProject }
    if (!(ValidProject $ProjectDir)) {
        throw "Select the original project containing tools\out\protos.h and apk\lib\armeabi\libDeadSpace.so and apk\assets."
    }
    $ProjectDir=(Resolve-Path -LiteralPath $ProjectDir).Path
    $work=Join-Path $ProjectDir "windows-build"
    $stage=Join-Path $work "host-src"
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    foreach($name in $required) {
        Copy-Item -LiteralPath (Join-Path $src $name) -Destination (Join-Path $stage $name) -Force
    }
    Write-Host "[Dead Space] Staged Windows host separately. Original game and translated C preserved."
    $projectWsl=((& wsl.exe --exec wslpath -a -u $ProjectDir | Select-Object -First 1) | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or !$projectWsl.StartsWith("/")) { throw "WSL project path mapping failed." }
    $stageWsl=((& wsl.exe --exec wslpath -a -u $stage | Select-Object -First 1) | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or !$stageWsl.StartsWith("/")) { throw "WSL source path mapping failed." }
    & wsl.exe --exec bash -lc "command -v i686-w64-mingw32-gcc-posix >/dev/null 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[Dead Space] Installing native Windows x86 compiler within WSL."
        & wsl.exe --user root --exec bash -lc "apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y gcc-mingw-w64-i686-posix binutils-mingw-w64-i686 python3 curl tar file"
        if ($LASTEXITCODE -ne 0) { throw "WSL could not install the Windows cross compiler." }
    }
    Write-Host "[Dead Space] Building the native Windows game using your locally generated source."
    & wsl.exe --exec bash "$stageWsl/build_win32_lifted.sh" "$projectWsl"
    if ($LASTEXITCODE -ne 0) {
        throw "Windows build failed. See windows-build\compile-windows.log. WSL game files were not changed."
    }
    $exe=Join-Path $work "DeadSpaceMobile-Win32.exe"
    if (!(Test-Path -LiteralPath $exe) -or !(Test-Path -LiteralPath (Join-Path $work "SDL2.dll")) -or !(Test-Path -LiteralPath (Join-Path $work "libwinpthread-1.dll"))) {
        throw "The native Windows executable or required DLLs are missing."
    }
    $play=Join-Path $work "Play Dead Space Windows.cmd"
    $playText=@'
@echo off
setlocal
title Dead Space Mobile - native Windows x86
cd /d "%~dp0"
if not exist logs mkdir logs
set "DS_WINDOW=1"
set "DS_STEPS=create,surface,frame"
set "DS_FRAMES=0"
set "DS_ASSETS=%~dp0..\apk\assets"
set "DS_DATA=%~dp0..\apk\assets"
set "DS_MODEL=R800i"
set "DS_MANUFACTURER=Sony Ericsson"
echo Starting native Windows game...
"DeadSpaceMobile-Win32.exe" "%~dp0..\apk\lib\armeabi\libDeadSpace.so" > "logs\windows-run.log" 2>&1
set "rc=%errorlevel%"
echo Exit code: %rc%
if not "%rc%"=="0" (
  echo Error log: "%~dp0logs\windows-run.log"
  start "" notepad "%~dp0logs\windows-run.log"
  pause
)
'@
    Set-Content -LiteralPath $play -Value $playText -Encoding Ascii
    Write-Host "[Dead Space] Built private executable: $exe"
    Inform "Native Windows x86 executable built from your game translation. Press OK to launch it. If a crash occurs, send windows-build\logs\windows-run.log." $false
    Start-Process -FilePath $play -WorkingDirectory $work
    exit 0
} catch {
    Write-Host "[Dead Space] $($_.Exception.Message)" -ForegroundColor Red
    Inform $_.Exception.Message $true
    exit 1
}
