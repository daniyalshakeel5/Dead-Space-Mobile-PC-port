[CmdletBinding()]
param(
    [string]$ApkPath = "",
    [string]$InstallDir = "",
    [switch]$PrepareOnly,
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"
$script:TranscriptStarted = $false
$script:InstallLog = ""

function StopInstallLog {
    if ($script:TranscriptStarted) {
        try { Stop-Transcript | Out-Null } catch {}
        $script:TranscriptStarted = $false
    }
}

function Notice([string]$Message,[bool]$IsError) {
    if ($PrepareOnly) { Write-Host $Message; return }
    Add-Type -AssemblyName System.Windows.Forms
    $ico = if($IsError) { [System.Windows.Forms.MessageBoxIcon]::Error }
           else { [System.Windows.Forms.MessageBoxIcon]::Information }
    [void][System.Windows.Forms.MessageBox]::Show(
        $Message,"Dead Space Mobile - Fresh Windows Install",
        [System.Windows.Forms.MessageBoxButtons]::OK,$ico)
}
function PickApk {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog=New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title="Select your original Dead Space Mobile APK (libDeadSpace.so edition)"
    $dialog.Filter="Android APK files (*.apk)|*.apk|ZIP archives (*.zip)|*.zip"
    $dialog.CheckFileExists=$true
    $dialog.Multiselect=$false
    $dialog.InitialDirectory=Join-Path $env:USERPROFILE "Downloads"
    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        throw "No APK selected. The installer did not change anything."
    }
    return $dialog.FileName
}
function WslPath([string]$WinPath) {
    $lines = & wsl.exe --exec wslpath -a -u $WinPath
    if ($LASTEXITCODE -ne 0 -or !$lines) {
        throw "WSL cannot access the selected Windows folder: $WinPath"
    }
    $first = (($lines | Select-Object -First 1) | Out-String).Trim()
    if (!$first.StartsWith("/")) { throw "Invalid WSL path: $first" }
    return $first
}
function CheckApk([string]$Path) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=[System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $seen=New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::OrdinalIgnoreCase)
        $dataSize=[long]0
        $hasLibrary=$false
        $hasAssets=$false
        $hasIni=$false
        foreach($entry in $zip.Entries) {
            $name=$entry.FullName.Replace('\','/')
            $parts=$name.Split('/')
            if ([string]::IsNullOrWhiteSpace($name) -or
                $name.StartsWith("/") -or
                $name.StartsWith("\") -or
                $name.Contains(":") -or
                ($parts -contains "..") -or
                ($parts -contains ".")) {
                throw "Unsafe APK archive entry found: $name"
            }
            if ($entry.Name.Length -eq 0) { continue }
            if (!$seen.Add($name)) { throw "APK contains duplicate filenames: $name" }
            $mode = ($entry.ExternalAttributes -shr 16) -band 0xF000
            if ($mode -eq 0xA000) { throw "APK contains an unsupported symbolic link: $name" }
            if ($entry.Length -gt 1073741824) { throw "APK contains a file exceeding 1 GiB: $name" }
            $dataSize += $entry.Length
            if ($dataSize -gt 3221225472L) { throw "APK expands beyond the 3 GiB safety limit." }
            if ($name.Equals("lib/armeabi/libDeadSpace.so",[StringComparison]::OrdinalIgnoreCase)) {
                $hasLibrary=$true
            }
            if ($name.StartsWith("assets/",[StringComparison]::OrdinalIgnoreCase)) { $hasAssets=$true }
            if ($name.Equals("assets/EAMCore.ini",[StringComparison]::OrdinalIgnoreCase)) { $hasIni=$true }
        }
        if (!$hasLibrary) {
            throw "This APK does not contain lib/armeabi/libDeadSpace.so. The Xperia Play libEAMGameDeadSpace.so edition is incompatible with this translator. Please select the matching Dead Space Mobile Fixes APK."
        }
        if (!$hasAssets -or !$hasIni) {
            throw "The selected APK lacks the original assets/EAMCore.ini resource layout. Select the matching original APK."
        }
        Write-Host "[DS INSTALL] Verified matching ARM library and asset layout; $($seen.Count) files."
    } finally { $zip.Dispose() }
}
function ExtractApk([string]$Path,[string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $base=[IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
    $zip=[System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        foreach($entry in $zip.Entries) {
            if (!$entry.Name) { continue }
            $rel=$entry.FullName.Replace('/','\')
            $target=[IO.Path]::GetFullPath((Join-Path $Destination $rel))
            if (!$target.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)) {
                throw "APK extraction path escapes install folder: $rel"
            }
            [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
            $stream=$entry.Open()
            try {
                $file=[IO.File]::Create($target)
                try { $stream.CopyTo($file) } finally { $file.Dispose() }
            } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
}
function NativeWslSetup {
    Write-Host "[DS INSTALL] Checking existing WSL toolchain..."
    & wsl.exe --exec bash -c 'command -v i686-w64-mingw32-gcc-posix >/dev/null 2>&1 && python3 -c "import elftools" >/dev/null 2>&1 && command -v curl >/dev/null 2>&1 && command -v file >/dev/null 2>&1'
    if ($LASTEXITCODE -eq 0) { return }
    Write-Host "[DS INSTALL] Installing Windows cross compiler and ELF translation dependencies in WSL..."
    & wsl.exe --user root --exec bash -c 'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y python3 python3-pyelftools gcc-mingw-w64-i686-posix binutils-mingw-w64-i686 curl tar file'
    if ($LASTEXITCODE -ne 0) {
        throw "Could not install compiler/pyelftools in WSL. Your extracted game files are preserved."
    }
}
function CreateLauncher([string]$GameRoot) {
    $work=Join-Path $GameRoot "windows-build"
    $play=Join-Path $work "Play Dead Space Windows.cmd"
    $source=@'
@echo off
setlocal
title Dead Space Mobile - Native Windows x86
cd /d "%~dp0"
if not exist logs mkdir logs
if not exist "%~dp0..\apk\assets\var" mkdir "%~dp0..\apk\assets\var"
if not exist "%~dp0..\apk\assets\var" echo [DS SAVE ERROR] Cannot create app var directory.
set "DS_WINDOW=1"
set "DS_STEPS=create,surface,frame"
set "DS_FRAMES=0"
set "DS_NATIVE_BUFFERED_IO=1"
set "DS_M3G_SERIALIZE=1"
set "DS_QUIET_JAVA=1"
set "DS_ASSETS=%~dp0..\apk\assets"
set "DS_DATA=%~dp0..\apk\assets"
set "DS_MODEL=R800i"
set "DS_MANUFACTURER=Sony Ericsson"
echo Launching Dead Space Mobile on native Windows x86...
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
    Set-Content -LiteralPath $play -Value $source -Encoding Ascii
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "Diagnose-DeadSpace-Crash.ps1") -Destination (Join-Path $work "Diagnose-DeadSpace-Crash.ps1") -Force
    return $play
}

try {
    $package=Split-Path -Parent $MyInvocation.MyCommand.Path
    $sourceTools=Join-Path $package "tools"
    $toolFiles=@(
        "rt.h","host.c","jni_vm.c","threads.c","libc_files.c","gl_shim.c",
        "front_sdl.c","build.sh","build_win32_lifted.sh",
        "gen_all.py","gen_gl.py","lift.py","overrides.py","jninames.py",
        "texturepack_fault_guard.py","modelcache_empty_vector.py","object3d_duplicate_probe.py",
        "native_buffered_input.py","m3g_loader_serialization.py","windows_lifted_compat.h","win32_bionic_openflags.h",
        "win32_substrate.h","win32_substrate.c",
        "win32_import_bridge.h","win32_missing_imports.c","win32_missing_gles.c"
    )
    foreach($name in $toolFiles) {
        if (!(Test-Path -LiteralPath (Join-Path $sourceTools $name))) {
            throw "Incomplete fresh installer download: tools\$name is missing."
        }
    }
    if (!$ApkPath) { $ApkPath=PickApk }
    if (!(Test-Path -LiteralPath $ApkPath -PathType Leaf)) {
        throw "The selected APK could not be found: $ApkPath"
    }
    $ApkPath=(Resolve-Path -LiteralPath $ApkPath).Path
    Write-Host "[DS INSTALL] Reading original APK: $ApkPath"
    CheckApk $ApkPath

    if (!$InstallDir) {
        $docs=[Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
        if (!$docs) { $docs=Join-Path $env:USERPROFILE "Documents" }
        $InstallDir=Join-Path $docs "DeadSpaceMobilePC"
    }
    $InstallDir=[IO.Path]::GetFullPath($InstallDir)
    if (Test-Path -LiteralPath $InstallDir) {
        $contents=@(Get-ChildItem -LiteralPath $InstallDir -Force -ErrorAction Stop)
        if ($contents.Count -gt 0) {
            throw "Install folder already contains files: $InstallDir. I will not overwrite them. Select another empty directory."
        }
    }
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    $script:InstallLog=Join-Path $InstallDir "install.log"
    Start-Transcript -Path $script:InstallLog -Append | Out-Null
    $script:TranscriptStarted=$true
    Write-Host "[DS INSTALL] Installation report: $script:InstallLog"
    $apkDir=Join-Path $InstallDir "apk"
    $toolsDir=Join-Path $InstallDir "tools"
    New-Item -ItemType Directory -Path $apkDir,$toolsDir -Force | Out-Null

    Write-Host "[DS INSTALL] Extracting your original APK into $apkDir..."
    ExtractApk $ApkPath $apkDir
    $originalSo=Join-Path $apkDir "lib\armeabi\libDeadSpace.so"
    $originalAssets=Join-Path $apkDir "assets"
    if (!(Test-Path -LiteralPath $originalSo -PathType Leaf) -or
        !(Test-Path -LiteralPath $originalAssets -PathType Container)) {
        throw "APK extraction did not create the expected original library and assets."
    }
    # Do not attempt to translate a different Android ABI or renamed game file.
    # ARM ELF32 (EM_ARM=40) and little-endian layout are required by lift.py.
    $header=New-Object byte[] 20
    $elf=[IO.File]::OpenRead($originalSo)
    try { $num=$elf.Read($header,0,$header.Length) } finally { $elf.Dispose() }
    if ($num -lt 20 -or $header[0] -ne 127 -or $header[1] -ne 69 -or
        $header[2] -ne 76 -or $header[3] -ne 70 -or
        $header[4] -ne 1 -or $header[5] -ne 1 -or
        [BitConverter]::ToUInt16($header,18) -ne 40) {
        throw "The extracted libDeadSpace.so is not the expected ARM32 little-endian ELF. Select the original matching APK, not an Xperia Play or ARM64 version."
    }
    $sha=(Get-FileHash -LiteralPath $originalSo -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath (Join-Path $InstallDir "original-elf-sha256.txt") -Value $sha
    Write-Host "[DS INSTALL] Original libDeadSpace.so SHA256: $sha"
    foreach($name in $toolFiles) {
        Copy-Item -LiteralPath (Join-Path $sourceTools $name) -Destination (Join-Path $toolsDir $name)
    }
    $report=@"
Dead Space Mobile private native Windows install
Original APK: $ApkPath
Original libDeadSpace.so SHA256: $sha
Install directory: $InstallDir
Game library and asset files remain local; never uploaded.
"@
    Set-Content -LiteralPath (Join-Path $InstallDir "INSTALL-REPORT.txt") -Value $report
    Write-Host "[DS INSTALL] Installed complete ARM-to-C translator and Win32 host source."
    if ($PrepareOnly) {
        Write-Host "[DS INSTALL] PASS: fresh package prepared (no WSL or game compilation requested)."
        StopInstallLog
        exit 0
    }

    if (!(Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        throw "WSL is not installed. A Linux WSL distribution is required only to compile the Windows executable."
    }
    NativeWslSetup
    $wslTools=WslPath $toolsDir
    $wslGame=WslPath $InstallDir
    Write-Host "[DS INSTALL] Regenerating original game's translated C code from APK..."
    # The argument is passed separately, never a Windows path inside Bash.
    & wsl.exe --exec bash -c 'cd "$1" && DS_TEXTUREPACK_DIAG=0 python3 gen_all.py out' _ $wslTools
    if ($LASTEXITCODE -ne 0) {
        throw "Translation of libDeadSpace.so failed. See output above. The extracted APK and source remain in $InstallDir."
    }
    if (!(Test-Path -LiteralPath (Join-Path $toolsDir "out\protos.h"))) {
        throw "ARM-to-C translation did not generate tools\out\protos.h."
    }
    Write-Host "[DS INSTALL] Compiling the native x86 Windows executable..."
    & wsl.exe --exec bash "$wslTools/build_win32_lifted.sh" "$wslGame"
    if ($LASTEXITCODE -ne 0) {
        throw "The Windows game build failed; send $InstallDir\windows-build\compile-windows.log. Your original APK and translated code are preserved."
    }
    $exe=Join-Path $InstallDir "windows-build\DeadSpaceMobile-Win32.exe"
    foreach($file in @($exe,(Join-Path $InstallDir "windows-build\SDL2.dll"),
                       (Join-Path $InstallDir "windows-build\libwinpthread-1.dll"))) {
        if (!(Test-Path -LiteralPath $file)) { throw "Windows build missing expected binary: $file" }
    }
    $play=CreateLauncher $InstallDir
    Notice "Windows x86 executable built in $InstallDir. The game has not yet been verified to boot on your PC. Press OK to open the test version." $false
    if (!$NoLaunch) {
        Start-Process -FilePath $play -WorkingDirectory (Join-Path $InstallDir "windows-build")
    }
    StopInstallLog
    exit 0
} catch {
    $msg=$_.Exception.Message
    Write-Host "[DS INSTALL ERROR] $msg" -ForegroundColor Red
    if ($script:InstallLog) {
        Write-Host "[DS INSTALL] Error log: $script:InstallLog"
    }
    StopInstallLog
    Notice ($msg + $(if ($script:InstallLog) { "`nInstaller log: $script:InstallLog" } else { "" })) $true
    exit 1
}
