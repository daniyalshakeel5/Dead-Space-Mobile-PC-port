param(
    [Parameter(Mandatory=$true)][string]$PackageZip,
    [Parameter(Mandatory=$true)][string]$Apk,
    [Parameter(Mandatory=$true)][string]$MesaArchive,
    [Parameter(Mandatory=$true)][string]$WorkRoot,
    [string]$ReanimatedApk,
    [switch]$AudioProbeOnly
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
$env:DS_FULLSCREEN = '0'
$env:DS_RENDER_MIN_FAR = '1200'
$env:DS_REANIMATED_TOUCH = '1'
$env:DS_HD_TEXTURES = '0'
$env:DS_QUIET_JAVA = '1'
$env:DS_MANUFACTURER = 'Generic'
$env:DS_MODEL = 'WindowsNative'
$env:DS_ACCEL_DIAG = '1'
$env:DS_NOVSYNC = '1'
$env:SDL_AUDIODRIVER = 'dummy'
$env:GALLIUM_DRIVER = 'llvmpipe'
$env:LIBGL_ALWAYS_SOFTWARE = '1'
$so = (Join-Path $root 'game/lib/armeabi/libDeadSpace.so').Replace('\','/')
$labels = @('audio-selftest','first-start','existing-install')
if ($ReanimatedApk) { $labels += 'reanimated-hd' }
$labels += 'native-controller'
if ($AudioProbeOnly) { $labels = @('audio-selftest','first-start') }
foreach ($label in $labels) {
    $env:DS_FULLSCREEN = if ($label -eq 'first-start') { '1' } else { '0' }
    if ($label -eq 'first-start') { Remove-Item Env:DS_FULLSCREEN }
    if ($label -eq 'existing-install') { $env:DS_KEY_HOLDS = '300:302:F11,600:602:F11' } else { Remove-Item Env:DS_KEY_HOLDS -ErrorAction SilentlyContinue }
    if ($label -eq 'audio-selftest') { $env:DS_AUDIO_SELFTEST = '1' }
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
    $timeout = 180000
    if ($label -eq 'native-controller') {
        $env:DS_FRAMES = '14000'
        $env:DS_HD_TEXTURES = '0'
        $env:DS_INPUT_DIAG = '1'
        $env:DS_NATIVE_PAD_TEST = '1'
        $presses = @(600,1200,1800,2400,3000,3600,4200,4800,5400,6000,6600,7200,7800,8400,9000,9600,10200)
        $env:DS_CONTROLLER_BUTTONS = (($presses | ForEach-Object { "${_}:$($_ + 20):a" }) -join ',') + ',8800:8820:start,12000:12500:leftshoulder,13200:13220:rightshoulder'
        $env:DS_CONTROLLER_AXES = '11000:11800:0:-24000:0:0,12000:12800:18000:0:10000:0'
        $env:DS_CONTROLLER_TRIGGERS = '13500:13520:-32768:32767'
        $env:DS_SHOT = '14000:' + $frame.Replace('\','/')
        $env:DS_SHOT_EVERY = '2000'
        $env:DS_SHOT_DIR = $evidence.Replace('\','/')
        $timeout = 600000
    }
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = Join-Path $root 'DeadSpaceMobile-Win32.exe'
    $info.WorkingDirectory = $root
    $info.Arguments = '"' + $so + '"'
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    if (!$process.Start()) { throw "$label could not start." }
    $outputTask = $process.StandardOutput.ReadToEndAsync()
    $errorTask = $process.StandardError.ReadToEndAsync()
    $finished = $process.WaitForExit($timeout)
    if (!$finished) { $process.Kill(); $process.WaitForExit() }
    [System.IO.File]::WriteAllText($stdout,$outputTask.Result)
    [System.IO.File]::WriteAllText($stderr,$errorTask.Result)
    $exitCode = $process.ExitCode
    $process.Dispose()
    $exitCode | Set-Content (Join-Path $evidence ($label + '-exit-code.txt'))
    Get-Content $stdout -Tail 30
    Get-Content $stderr -Tail 45
    if (!$finished) { throw "$label timed out after $($timeout / 1000) seconds." }
    if ($exitCode -ne 0) { throw "$label exited with $exitCode." }
    if ($label -eq 'audio-selftest') {
        if (!(Select-String -LiteralPath $stderr -SimpleMatch '[AUDIO_SELFTEST]')) { throw 'Audio timing self-test did not run.' }
        Remove-Item Env:DS_AUDIO_SELFTEST
        continue
    }
    if (!(Select-String -LiteralPath $stdout -Pattern '^ran frames')) { throw "$label did not complete the frame loop." }
    $displayLine = Select-String -LiteralPath $stderr -Pattern '\[DISPLAY\].*desktop=(\d+)x(\d+).*fullscreen=(\d) drawable=(\d+)x(\d+)' | Select-Object -First 1
    if (!$displayLine) { throw "$label did not report display detection." }
    $display = $displayLine.Matches[0].Groups
    if ($label -eq 'first-start') {
        if ($display[3].Value -ne '1' -or $display[1].Value -ne $display[4].Value -or $display[2].Value -ne $display[5].Value) { throw 'Default fullscreen does not match the detected desktop resolution.' }
    } elseif ($display[3].Value -ne '0' -or $display[4].Value -ne '1366' -or $display[5].Value -ne '768') { throw "$label did not create the expected windowed regression surface." }
    ($display[4].Value + ' ' + $display[5].Value) | Set-Content (Join-Path $evidence ($label + '-frame-size.txt'))
    if ($label -eq 'existing-install') {
        $toggles = @(Select-String -LiteralPath $stderr -Pattern '\[DISPLAY_TOGGLE\] fullscreen=(\d)')
        if ($toggles.Count -ne 2 -or $toggles[0].Matches[0].Groups[1].Value -ne '1' -or $toggles[1].Matches[0].Groups[1].Value -ne '0') { throw 'F11 fullscreen/windowed round trip failed.' }
    }
    if (!(Select-String -LiteralPath $stderr -SimpleMatch '[WINDOWS_ACCEL] initialized stationary logical sensor')) { throw "$label did not initialize the Windows sensor bridge." }
    if (Select-String -LiteralPath $stderr -SimpleMatch 'Windows SEH exception') { throw "$label reported a Windows exception." }
    if (!(Test-Path -LiteralPath $frame)) { throw "$label framebuffer capture is missing." }
    $audioClock = Select-String -LiteralPath $stderr -Pattern '\[AUDIO_CLOCK\].*empty_writes=(\d+)' | Select-Object -Last 1
    if (!$audioClock) { throw "$label audio clock evidence is missing." }
    if ([int]$audioClock.Matches[0].Groups[1].Value -gt 8) { throw "$label audio repeatedly ran out of queued PCM." }
    $audioRate = Select-String -LiteralPath $stderr -Pattern '\[AUDIO_CLOCK\].*nominal_rate=(\d+) measured_rate=([\d.]+)' | Select-Object -Last 1
    if (!$audioRate -or [double]::Parse($audioRate.Matches[0].Groups[2].Value,[System.Globalization.CultureInfo]::InvariantCulture) -lt [int]$audioRate.Matches[0].Groups[1].Value * 0.9) { throw "$label audio delivery remains below 90 percent of its nominal rate." }
    if ($label -eq 'native-controller') {
        if (!(Select-String -LiteralPath $stderr -SimpleMatch '[NATIVE_PAD_CONTINUE]')) { throw 'No native loading-screen activation was observed.' }
        if (!(Select-String -LiteralPath $stderr -SimpleMatch '[NATIVE_PAD_MENU]')) { throw 'No native menu activation was observed.' }
        if (!(Select-String -LiteralPath $stderr -Pattern '\[NATIVE_PAD\].*active=1 move=(?!0\.000,0\.000)')) { throw 'Native controller did not reach gameplay movement.' }
        if (!(Select-String -LiteralPath $stderr -SimpleMatch '[NATIVE_PAD_STOMP]')) { throw 'Native stomp was not dispatched.' }
        if (Select-String -LiteralPath $stderr -Pattern '\[LEGACY_INPUT\] (key|pointer kind=[01])') { throw 'Controller test generated legacy touch/key input.' }
    }
}
$summary = if ($AudioProbeOnly) { 'PASS: Windows audio timing probe and original-game startup: ' } else { 'PASS: Windows startup plus 14000-frame native-controller run, without legacy touch/key input: ' }
($summary + ($labels -join ', ')) | Set-Content (Join-Path $evidence 'result.txt')
