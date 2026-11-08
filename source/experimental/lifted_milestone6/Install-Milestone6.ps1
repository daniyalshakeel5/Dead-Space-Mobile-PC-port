[CmdletBinding()]
param(
    [string]$ProjectDir = "",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Test-DeadSpaceProject([string]$candidate) {
    if (-not $candidate) { return $false }
    return (Test-Path -LiteralPath (Join-Path $candidate "tools\gen_all.py")) -and
           (Test-Path -LiteralPath (Join-Path $candidate "apk\lib\armeabi\libDeadSpace.so"))
}

function Find-DeadSpaceProject {
    $direct = Join-Path $env:USERPROFILE "Downloads\dspc_milestone5_one_click_vscode\dspc"
    if (Test-DeadSpaceProject $direct) { return $direct }
    $downloads = Join-Path $env:USERPROFILE "Downloads"
    foreach ($dir in (Get-ChildItem -LiteralPath $downloads -Directory -Filter "dspc" -Recurse -ErrorAction SilentlyContinue)) {
        if (Test-DeadSpaceProject $dir.FullName) { return $dir.FullName }
    }
    throw "Could not find your current WSL Dead Space project. No files changed."
}

function Info([string]$message) {
    Write-Host "[Dead Space M6] $message"
}

try {
    $sourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    $sourceTools = Join-Path $sourceRoot "tools"
    if (-not (Test-Path -LiteralPath (Join-Path $sourceTools "texturepack_fault_guard.py"))) {
        throw "The milestone 6 package is incomplete: missing texture diagnostics."
    }
    if ([string]::IsNullOrWhiteSpace($ProjectDir)) { $ProjectDir = Find-DeadSpaceProject }
    $ProjectDir = (Resolve-Path -LiteralPath $ProjectDir).Path
    if (-not (Test-DeadSpaceProject $ProjectDir)) {
        throw "This folder is not a matching libDeadSpace.so project: $ProjectDir. No files changed."
    }
    $targetTools = Join-Path $ProjectDir "tools"
    if ([IO.Path]::GetFullPath($sourceTools).TrimEnd("\") -ieq
        [IO.Path]::GetFullPath($targetTools).TrimEnd("\")) {
        throw "Extract the update ZIP outside your existing dspc folder first."
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $backup = Join-Path $ProjectDir ("backups\pre-m6-asset-trace-" + $stamp)
    $backupTools = Join-Path $backup "tools"
    New-Item -ItemType Directory -Path $backupTools -Force | Out-Null

    # Never copy/overwrite any original game binary, asset folder or save file.
    $sourceFiles = @(Get-ChildItem -LiteralPath $sourceTools -File)
    if ($sourceFiles.Count -lt 18) {
        throw "Too few host tool files in the update; refusing partial installation."
    }
    foreach ($file in $sourceFiles) {
        $target = Join-Path $targetTools $file.Name
        if (Test-Path -LiteralPath $target) {
            Copy-Item -LiteralPath $target -Destination (Join-Path $backupTools $file.Name) -Force
        }
    }

    # Archive stale generated translated code instead of deleting it. This
    # forces the existing one-click build to regenerate with two-point tracing.
    $out = Join-Path $targetTools "out"
    if (Test-Path -LiteralPath $out) {
        Move-Item -LiteralPath $out -Destination (Join-Path $backup "out")
        Info "Existing translated C/executables moved to a reversible backup."
    }
    foreach ($file in $sourceFiles) {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $targetTools $file.Name) -Force
    }
    Info ("Updated " + $sourceFiles.Count + " asset-free host tool files.")
    Info "Original libDeadSpace.so, APK assets and saves were not modified."
    Info "Backup: $backup"
    if (-not $Quiet) {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show(
            "Milestone 6 host source installed. The next launch will rebuild translated C, trace the TexturePack object vector, and log missing native resource paths." +
            [Environment]::NewLine + [Environment]::NewLine +
            "Your original APK, assets, and saves are untouched.",
            "Dead Space Mobile — Milestone 6 updated",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Information)
        $play = Join-Path $ProjectDir "Play Dead Space (WSL).cmd"
        if (Test-Path -LiteralPath $play) {
            Info "Starting your existing Windows-to-WSL launcher."
            Start-Process -FilePath $play -WorkingDirectory $ProjectDir
        } else {
            Info "Launch your existing VS Code Run and Debug > Play Dead Space configuration."
        }
    }
    exit 0
} catch {
    Write-Host ("Milestone 6 update failed safely: " + $_.Exception.Message) -ForegroundColor Red
    if (-not $Quiet) {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message, "Dead Space Mobile — Update error",
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error)
    }
    exit 1
}
