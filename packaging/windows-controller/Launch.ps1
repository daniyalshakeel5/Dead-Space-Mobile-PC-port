param([string]$Apk, [switch]$PrepareOnly, [switch]$RenderDiagnostics, [switch]$InputDiagnostics, [switch]$DisableFog, [switch]$DisableCull, [double]$RenderMinFar = 0)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$expected = '64a3323cd3cc0c483a645b62af8508624163ba9b'
$game = Join-Path $root 'game'
$so = Join-Path $game 'lib/armeabi/libDeadSpace.so'
$logs = Join-Path $root 'logs'
New-Item -ItemType Directory -Force $logs | Out-Null
try {
    foreach ($name in @('DeadSpaceMobile-Win32.exe','SDL2.dll','libwinpthread-1.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $root $name))) { throw "Missing $name. Extract the entire ZIP before launching." }
    }
    if (!(Test-Path -LiteralPath $so)) {
        if (Test-Path -LiteralPath $game) { throw 'Incomplete game folder. Rename the game folder before trying setup again; keep any saves inside it.' }
        if (!$Apk) {
            Add-Type -AssemblyName System.Windows.Forms
            $picker = New-Object System.Windows.Forms.OpenFileDialog
            $picker.Title = 'Select your Dead Space Mobile Fixes APK (libDeadSpace.so edition)'
            $picker.Filter = 'Android package (*.apk)|*.apk'
            try {
                if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit 0 }
                $Apk = $picker.FileName
            } finally { $picker.Dispose() }
        }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Apk).Path)
        $stage = Join-Path $root ('setup-' + [guid]::NewGuid().ToString('N'))
        try {
            $entries = @($archive.Entries | Where-Object { $_.FullName -eq 'lib/armeabi/libDeadSpace.so' })
            if ($entries.Count -ne 1) { throw 'Wrong APK edition: expected lib/armeabi/libDeadSpace.so. Xperia libEAMGameDeadSpace.so is not compatible with this build.' }
            $stream = $entries[0].Open()
            $sha = [System.Security.Cryptography.SHA1]::Create()
            try { $identity = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
            finally { $stream.Dispose(); $sha.Dispose() }
            if ($identity -ne $expected) { throw "APK library does not match this tested build. Found SHA1 $identity" }
            New-Item -ItemType Directory -Path $stage | Out-Null
            $prefix = [System.IO.Path]::GetFullPath($stage) + [System.IO.Path]::DirectorySeparatorChar
            foreach ($entry in $archive.Entries) {
                $name = $entry.FullName.Replace('\','/')
                if (!($name.StartsWith('assets/') -or $name -eq 'lib/armeabi/libDeadSpace.so')) { continue }
                $dest = [System.IO.Path]::GetFullPath((Join-Path $stage $name))
                if (!$dest.StartsWith($prefix,[System.StringComparison]::OrdinalIgnoreCase) -or $name.Contains(':')) { throw 'Unsafe archive path.' }
                if ($name.EndsWith('/')) { New-Item -ItemType Directory -Force $dest | Out-Null; continue }
                New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($dest)) | Out-Null
                [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$dest,$false)
            }
            if (!(Test-Path -LiteralPath (Join-Path $stage 'assets/published'))) { throw 'APK is missing the published game assets.' }
            Move-Item -LiteralPath $stage -Destination $game
        } finally {
            $archive.Dispose()
            if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
        }
    }
    if ((Get-FileHash -LiteralPath $so -Algorithm SHA1).Hash.ToLowerInvariant() -ne $expected) { throw 'Installed game library is not the matching tested version.' }
    if (!(Test-Path -LiteralPath (Join-Path $game 'assets/published'))) { throw 'Installed game assets are missing.' }
    New-Item -ItemType Directory -Force (Join-Path $game 'assets/var') | Out-Null
    if ($PrepareOnly) { Write-Host 'Matching game files prepared successfully.'; exit 0 }
    # Clear inherited diagnostic/test switches. Real input and unlimited play.
    Get-ChildItem Env: | Where-Object { $_.Name -like 'DS_*' } | ForEach-Object { Remove-Item ('Env:' + $_.Name) }
    $env:DS_WINDOW = '1'
    $env:DS_STEPS = 'create,surface,frame'
    $env:DS_FRAMES = '0'
    $env:DS_QUIET_JAVA = '1'
    if ($RenderDiagnostics) { $env:DS_RENDER_DIAG = '1' }
    if ($InputDiagnostics) { $env:DS_INPUT_DIAG = '1' }
    if ($DisableFog) { $env:DS_RENDER_NO_FOG = '1' }
    if ($DisableCull) { $env:DS_RENDER_NO_CULL = '1' }
    if ($RenderMinFar -gt 0) {
        $env:DS_RENDER_MIN_FAR = $RenderMinFar.ToString([System.Globalization.CultureInfo]::InvariantCulture)
    }
    $env:DS_ASSETS = (Join-Path $game 'assets').Replace('\','/')
    # Use the same asset/data layout as the verified Windows gameplay run.
    $env:DS_DATA = $env:DS_ASSETS
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $stdout = Join-Path $logs "$stamp-stdout.log"
    $stderr = Join-Path $logs "$stamp-stderr.log"
    Write-Host 'Starting Dead Space Mobile. Left stick: move. Right stick: look.'
    Write-Host 'Close the game window to exit. Logs are saved in the logs folder.'
    $process = Start-Process -FilePath (Join-Path $root 'DeadSpaceMobile-Win32.exe') -WorkingDirectory $root -ArgumentList ('"' + $so + '"') -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -Wait
    $code = $process.ExitCode
    @("Exit code: $code", "Build: acd4c1800b7949841be7f5ac5d6c6fcb216ed95e", "Time: $(Get-Date -Format o)") | Set-Content (Join-Path $logs "$stamp-result.txt")
    if ($code -ne 0) { throw "Game exited with code $code. Please send the files in logs." }
} catch {
    $_ | Out-String | Tee-Object -FilePath (Join-Path $logs 'launcher-error.txt') | Write-Host
    exit 1
}
