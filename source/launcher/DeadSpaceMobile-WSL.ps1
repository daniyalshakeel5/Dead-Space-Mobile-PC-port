# Launch the packaged native Linux binary through WSLg; no source download/rebuild.
param([string]$ApkPath, [switch]$ChangeApk, [switch]$Windowed)
$ErrorActionPreference = 'Stop'
try {
    $script = Join-Path $PSScriptRoot 'Play-Dead-Space-Touch.sh'
    $binary = Join-Path $PSScriptRoot 'dspc_sdl'
    if (!(Test-Path -LiteralPath $script) -or !(Test-Path -LiteralPath $binary)) {
        throw 'Extract the complete Linux release ZIP. Keep this launcher beside dspc_sdl and Play-Dead-Space-Touch.sh. A source-code ZIP is not the playable package.'
    }
    $wsl = Get-Command wsl.exe -ErrorAction Stop
    $linuxScript = (& $wsl.Source --exec wslpath -a -u $script).Trim()
    if ($LASTEXITCODE -ne 0 -or !$linuxScript.StartsWith('/')) {
        throw 'WSL is not ready. Install Ubuntu 24.04 under WSL2, complete setup, and install the dependencies in START-HERE.md.'
    }
    $hasGame = (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'game/lib/armeabi/libDeadSpace.so')) -and
               (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'game/assets/published'))
    if (!$ApkPath -and ($ChangeApk -or !$hasGame)) {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object System.Windows.Forms.OpenFileDialog
        $dialog.Title = 'Select Fixes APK or DeadSpaceReanimated-1.3.apk (HD textures)'
        $dialog.Filter = 'Android APK (*.apk)|*.apk|All files (*.*)|*.*'
        $dialog.CheckFileExists = $true
        if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
            $dialog.Dispose()
            Write-Host 'Cancelled.'
            exit 0
        }
        $ApkPath = $dialog.FileName
        $dialog.Dispose()
    }
    $launchArgs = @('--exec', 'bash', $linuxScript)
    if ($Windowed) { $launchArgs += '--windowed' }
    if ($ApkPath) {
        $resolvedApk = (Resolve-Path -LiteralPath $ApkPath).Path
        $linuxApk = (& $wsl.Source --exec wslpath -a -u $resolvedApk).Trim()
        if ($LASTEXITCODE -ne 0 -or !$linuxApk.StartsWith('/')) { throw 'WSL cannot access the selected APK.' }
        $launchArgs += $linuxApk
    }
    Write-Host 'Launching the bundled Linux build through WSLg.'
    & $wsl.Source @launchArgs
    exit $LASTEXITCODE
} catch {
    Write-Host ('ERROR: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
