param(
    [Parameter(Mandatory=$true)][string]$PackageZip,
    [Parameter(Mandatory=$true)][string]$Apk,
    [Parameter(Mandatory=$true)][string]$MesaArchive,
    [Parameter(Mandatory=$true)][string]$WorkRoot,
    [string]$ReanimatedApk
)
$ErrorActionPreference = 'Stop'
$evidence = Join-Path $WorkRoot 'evidence'
New-Item -ItemType Directory -Force $evidence | Out-Null
Expand-Archive -LiteralPath $PackageZip -DestinationPath (Join-Path $WorkRoot 'package')
$root = Join-Path $WorkRoot 'package/DeadSpaceMobile-Windows'
Get-FileHash -LiteralPath (Join-Path $root 'DeadSpaceMobile-Win32.exe') -Algorithm SHA256 | Format-List | Out-File (Join-Path $evidence 'executable-sha256.txt')
& powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $root 'Launch.ps1') -Apk $Apk -PrepareOnly
if ($LASTEXITCODE -ne 0) { throw 'Original APK preparation failed.' }
& 7z x $MesaArchive ('-o' + (Join-Path $WorkRoot 'mesa')) -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Mesa extraction failed.' }
$gl = Get-ChildItem (Join-Path $WorkRoot 'mesa') -Recurse -Filter opengl32.dll | Select-Object -First 1
if (!$gl) { throw 'Mesa x86 OpenGL library is missing.' }
Copy-Item (Join-Path $gl.DirectoryName '*.dll') $root
Get-ChildItem Env: | Where-Object { $_.Name -like 'DS_*' } | ForEach-Object { Remove-Item ('Env:' + $_.Name) }
$env:DS_ASSETS = (Join-Path $root 'game/assets').Replace('\','/')
$env:DS_DATA = (Join-Path $root 'userdata').Replace('\','/')
$env:DS_WINDOW = '1'
$env:DS_STEPS = 'create,surface,frame'
$env:DS_FRAMES = '1200'
$env:DS_WIDTH = '1366'
$env:DS_HEIGHT = '768'
$env:DS_RENDER_MIN_FAR = '1200'
$env:DS_REANIMATED_TOUCH = '1'
$env:DS_HD_TEXTURES = '0'
$env:DS_QUIET_JAVA = '1'
$env:DS_ACCEL_DIAG = '1'
$env:DS_NOVSYNC = '1'
$env:SDL_AUDIODRIVER = 'dummy'
$env:GALLIUM_DRIVER = 'llvmpipe'
$env:LIBGL_ALWAYS_SOFTWARE = '1'
$so = (Join-Path $root 'game/lib/armeabi/libDeadSpace.so').Replace('\','/')
$labels = @('first-start','existing-install')
if ($ReanimatedApk) { $labels += 'reanimated-hd' }
foreach ($label in $labels) {
    if ($label -eq 'reanimated-hd') {
        & powershell.exe -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $root 'Launch.ps1') -Apk $ReanimatedApk -PrepareOnly
        if ($LASTEXITCODE -ne 0) { throw 'Reanimated HD preparation failed.' }
        $env:DS_HD_TEXTURES = (Join-Path $root 'game/texture_hd').Replace('\','/')
        if (@(Get-ChildItem $env:DS_HD_TEXTURES -Filter '*.ktx').Count -lt 200) { throw 'The expected Reanimated HD pack is missing.' }
        $env:DS_HD_DIAG = '1'
    }
    $stdout = Join-Path $evidence ($label + '-stdout.log')
    $stderr = Join-Path $evidence ($label + '-stderr.log')
    $frame = Join-Path $evidence ($label + '.ppm')
    $env:DS_SHOT = '1200:' + $frame.Replace('\','/')
    $process = Start-Process -FilePath (Join-Path $root 'DeadSpaceMobile-Win32.exe') -WorkingDirectory $root -ArgumentList ('"' + $so + '"') -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (!$process.WaitForExit(180000)) {
        Stop-Process -Id $process.Id -Force
        throw "$label timed out after 180 seconds."
    }
    $process.Refresh()
    $process.ExitCode | Set-Content (Join-Path $evidence ($label + '-exit-code.txt'))
    Get-Content $stdout -Tail 30
    Get-Content $stderr -Tail 45
    if ($process.ExitCode -ne 0) { throw "$label exited with $($process.ExitCode)." }
    if (!(Select-String -LiteralPath $stdout -Pattern '^ran frames')) { throw "$label did not complete the frame loop." }
    if (!(Select-String -LiteralPath $stdout -Pattern 'window 1366x768, GL_RENDERER=')) { throw "$label did not create the expected OpenGL window." }
    if (!(Select-String -LiteralPath $stderr -SimpleMatch '[WINDOWS_ACCEL] initialized stationary logical sensor')) { throw "$label did not initialize the Windows sensor bridge." }
    if (Select-String -LiteralPath $stderr -SimpleMatch 'Windows SEH exception') { throw "$label reported a Windows exception." }
    if (!(Test-Path -LiteralPath $frame)) { throw "$label framebuffer capture is missing." }
}
('PASS: Windows starts completed 1200 real-game frames each: ' + ($labels -join ', ')) | Set-Content (Join-Path $evidence 'result.txt')
