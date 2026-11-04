# Dead Space Mobile PC - first-run setup and subsequent one-click WSL launch.
# All game binaries and assets are generated locally from the user's APK.
param([switch]$ChangeApk)
$ErrorActionPreference = 'Stop'
$repo = 'https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port.git'
$branch = 'work/deadspace-real-baseline-2026-11-05'
$source = '$HOME/DeadSpaceMobilePC-Source'
$install = '$HOME/DeadSpaceMobilePC-Linux'
$settingsDir = Join-Path $env:LOCALAPPDATA 'DeadSpaceMobilePC-WSL'
$apkRecord = Join-Path $settingsDir 'apk-path.txt'
try {
    New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
    $wsl = Get-Command wsl.exe -ErrorAction Stop
    $distros = @(& $wsl.Source --list --quiet 2>&1 | Where-Object { $_.ToString().Trim() })
    if ($LASTEXITCODE -ne 0 -or $distros.Count -eq 0) {
        throw 'No WSL distribution detected. Install Ubuntu in WSL and complete its first-run setup.'
    }
    Write-Host 'Dead Space Mobile - native Linux/WSL launcher' -ForegroundColor Cyan
    # Always refresh the source before launching an existing install. Prior
    # launcher versions skipped updates and kept the crashing executable.
    $savedApk = $null
    if ((Test-Path $apkRecord) -and -not $ChangeApk) {
        $savedApk = (Get-Content $apkRecord -Raw).Trim()
        if (-not (Test-Path $savedApk)) { $savedApk = $null }
    }
    $apk = $savedApk
    if (-not $apk) {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = 'Select your original Dead Space Mobile APK'
    $dialog.Filter = 'Android APK (*.apk)|*.apk|All files (*.*)|*.*'
    $dialog.CheckFileExists = $true
    if ((Test-Path $apkRecord) -and -not $ChangeApk) {
        $previous = (Get-Content $apkRecord -Raw).Trim()
        if (Test-Path $previous) { $dialog.FileName = $previous }
    }
    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        Write-Host 'Cancelled. No files changed.'
        exit 0
    }
    $apk = $dialog.FileName
    }

    # Use WSL's own wslpath to preserve Windows paths with spaces.
    $linuxApk = (& $wsl.Source --exec wslpath -u $apk).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $linuxApk.StartsWith('/')) {
        throw "WSL could not access the selected APK: $apk"
    }
    Set-Content -Path $apkRecord -Value $apk -Encoding UTF8
    Write-Host 'Downloading PC port source into your WSL home directory...' -ForegroundColor Cyan
    # Use positional arguments rather than interpolating selected paths into shell code.
    $bootstrap = @'
set -euo pipefail
if ! command -v git >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y git
fi
if [[ ! -d "$HOME/DeadSpaceMobilePC-Source/.git" ]]; then
  git clone --single-branch -b "$1" "$2" "$HOME/DeadSpaceMobilePC-Source"
else
  git -C "$HOME/DeadSpaceMobilePC-Source" pull --ff-only
fi
cd "$HOME/DeadSpaceMobilePC-Source"
revision=$(git rev-parse HEAD)
stamp="$HOME/DeadSpaceMobilePC-Linux/.source-revision"
if [[ "$4" != "1" && -x "$HOME/DeadSpaceMobilePC-Linux/play.sh" && -f "$stamp" && "$(cat "$stamp")" == "$revision" ]]; then
  exec "$HOME/DeadSpaceMobilePC-Linux/play.sh"
fi
# Source changed, or previous launcher did not record a revision: rebuild.
bash scripts/build-run-wsl.sh "$3"
printf '%s\n' "$revision" > "$stamp"
'@
    & $wsl.Source --exec bash -c $bootstrap - $branch $repo $linuxApk $(if ($ChangeApk) { "1" } else { "0" })
    exit $LASTEXITCODE
} catch {
    Write-Host ('ERROR: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
