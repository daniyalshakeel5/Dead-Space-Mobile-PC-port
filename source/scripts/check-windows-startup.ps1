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
$pe = [System.IO.File]::ReadAllBytes((Join-Path $root 'DeadSpaceMobile-Win32.exe'))
$peOffset = [BitConverter]::ToInt32($pe,0x3c)
if ([BitConverter]::ToUInt16($pe,$peOffset + 24 + 68) -ne 2) { throw 'The game must use the Windows GUI subsystem, not a console.' }
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
function Invoke-DesktopProbe($Game, [string]$OutDir) {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class DesktopProbe {
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct CursorInfo { public int Size,Flags; public IntPtr Cursor; public Point Position; }
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr window,ref Point point);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")] static extern int GetWindowLong(IntPtr window,int index);
    [DllImport("user32.dll")] static extern bool AdjustWindowRectEx(ref Rect rect,uint style,bool menu,uint extended);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int w,int h,uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
    [DllImport("user32.dll")] static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window,uint message,IntPtr key,IntPtr data);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint key,uint type);
    [DllImport("user32.dll")] static extern bool GetCursorInfo(ref CursorInfo info);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    public static Rectangle Client(IntPtr window) {
        Rect r; Point p=new Point();
        if(!GetClientRect(window,out r)||!ClientToScreen(window,ref p)) throw new Exception("Client rectangle unavailable");
        return new Rectangle(p.X,p.Y,r.Right-r.Left,r.Bottom-r.Top);
    }
    public static void Key(IntPtr window,uint key) {
        uint scan=MapVirtualKey(key,0)<<16;
        PostMessage(window,0x100,new IntPtr(key),new IntPtr((long)(scan|1)));
        PostMessage(window,0x101,new IntPtr(key),new IntPtr((long)(scan|0xc0000001u)));
    }
    public static void Click(int x,int y) { SetCursorPos(x,y); mouse_event(2,0,0,0,UIntPtr.Zero); mouse_event(4,0,0,0,UIntPtr.Zero); }
    public static bool CursorVisible() {
        CursorInfo info=new CursorInfo(); info.Size=Marshal.SizeOf(typeof(CursorInfo));
        return GetCursorInfo(ref info)&&(info.Flags&1)!=0;
    }
    public static void ResizeClient(IntPtr window,int width,int height) {
        Rect old; GetWindowRect(window,out old);
        Rect r=new Rect(); r.Right=width; r.Bottom=height;
        AdjustWindowRectEx(ref r,(uint)GetWindowLong(window,-16),false,(uint)GetWindowLong(window,-20));
        if(!SetWindowPos(window,IntPtr.Zero,old.Left,old.Top,r.Right-r.Left,r.Bottom-r.Top,0x14)) throw new Exception("Resize failed");
    }
    public static void Capture(IntPtr window,string path) {
        Rectangle r=Client(window);
        using(Bitmap bitmap=new Bitmap(r.Width,r.Height,PixelFormat.Format24bppRgb)) {
            using(Graphics graphics=Graphics.FromImage(bitmap)) graphics.CopyFromScreen(r.Location,System.Drawing.Point.Empty,r.Size);
            bitmap.Save(path,ImageFormat.Png);
        }
    }
}
'@
    [DesktopProbe]::SetThreadDpiAwarenessContext([IntPtr](-4)) | Out-Null
    $deadline = (Get-Date).AddSeconds(20)
    $gameWindow = [IntPtr]::Zero
    while ((Get-Date) -lt $deadline) {
        $Game.Refresh()
        if ($Game.HasExited) { throw 'Game exited before desktop checks.' }
        $gameWindow = $Game.MainWindowHandle
        if ($gameWindow -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 100
    }
    if ($gameWindow -eq [IntPtr]::Zero) { throw 'Game window was not created.' }
    $probe = New-Object System.Windows.Forms.Form
    $probe.Text = 'Desktop focus regression'
    $probe.StartPosition = 'Manual'; $probe.Location = New-Object System.Drawing.Point(10,10)
    $probe.Size = New-Object System.Drawing.Size(160,64)
    $probe.ShowInTaskbar = $false; $probe.TopMost = $true
    $probe.Cursor = [System.Windows.Forms.Cursors]::Arrow
    function Capture-Game([string]$Name) {
        [DesktopProbe]::SetForegroundWindow($gameWindow) | Out-Null
        $rect = [DesktopProbe]::Client($gameWindow)
        [DesktopProbe]::SetCursorPos($rect.Left+72,$rect.Top+60) | Out-Null
        Start-Sleep -Milliseconds 300
        [DesktopProbe]::Key($gameWindow,0x7b)
        $script:desktopProbeSerial++
        $number = $script:desktopProbeSerial
        $path = Join-Path $OutDir "cursor-$number.ppm"
        $until = (Get-Date).AddSeconds(15)
        while (!(Test-Path -LiteralPath $path) -and (Get-Date) -lt $until -and !$Game.HasExited) { Start-Sleep -Milliseconds 50 }
        if (!(Test-Path -LiteralPath $path)) { throw 'The game did not render a cursor probe after focus restoration.' }
        Start-Sleep -Milliseconds 150
        [DesktopProbe]::Capture($gameWindow,(Join-Path $OutDir ($Name+'.png')))
        ($rect.Width.ToString()+' '+$rect.Height) | Set-Content (Join-Path $OutDir "cursor-$number-frame-size.txt")
        return $rect
    }
    function Click-Away {
        $probe.Show(); $probe.Activate(); [System.Windows.Forms.Application]::DoEvents()
        [DesktopProbe]::Click(35,40)
        for ($pump=0;$pump -lt 4;$pump++) {
            [System.Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 100
        }
        if ($Game.HasExited) { throw 'Game exited when clicking another window.' }
        if ([DesktopProbe]::IsIconic($gameWindow)) { throw 'Focus loss minimized the game.' }
        if ($script:desktopProbeOsCursor -and ![DesktopProbe]::CursorVisible()) { throw 'The system cursor stayed hidden outside the game.' }
    }
    $script:desktopProbeSerial=0
    $script:desktopProbeOsCursor=$false
    $cursorShows=0
    try {
        # A headless runner can start with the OS cursor display count below zero.
        $probe.Show(); $probe.Activate(); [System.Windows.Forms.Application]::DoEvents()
        [DesktopProbe]::Click(35,40)
        for ($pump=0;$pump -lt 4;$pump++) { [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 50 }
        while (![DesktopProbe]::CursorVisible() -and $cursorShows -lt 8) {
            [System.Windows.Forms.Cursor]::Show(); $cursorShows++
            [System.Windows.Forms.Application]::DoEvents()
        }
        $script:desktopProbeOsCursor=[DesktopProbe]::CursorVisible()
        ("OS cursor baseline supported: "+$script:desktopProbeOsCursor) | Set-Content (Join-Path $OutDir 'desktop-cursor-baseline.txt')
        $probe.Hide()
        [void](Capture-Game 'desktop-windowed')
        Click-Away
        $probe.Hide(); [DesktopProbe]::SetForegroundWindow($gameWindow) | Out-Null
        [DesktopProbe]::Key($gameWindow,0x7a); Start-Sleep -Milliseconds 300
        $full = Capture-Game 'desktop-fullscreen'
        $screen = [System.Windows.Forms.Screen]::FromHandle($gameWindow).Bounds
        if ($full.Width -ne $screen.Width -or $full.Height -ne $screen.Height) { throw 'Borderless fullscreen does not cover the display.' }
        for ($cycle=0;$cycle -lt 3;$cycle++) {
            Click-Away
            $probe.Hide(); [DesktopProbe]::SetForegroundWindow($gameWindow) | Out-Null
            Start-Sleep -Milliseconds 200
        }
        [DesktopProbe]::Key($gameWindow,0x7a); Start-Sleep -Milliseconds 300
        Click-Away
        [DesktopProbe]::ResizeClient($gameWindow,900,540)
        Start-Sleep -Milliseconds 200
        $probe.Hide()
        $resized = Capture-Game 'desktop-restored'
        if ($resized.Width -ne 900 -or $resized.Height -ne 540) { throw 'Background resize did not survive focus restoration.' }
        Copy-Item (Join-Path $OutDir 'cursor-3.ppm') (Join-Path $OutDir 'desktop-integration.ppm')
        'PASS: real click-away cycles, unminimized fullscreen, external cursor visibility, background resize and desktop capture.' | Set-Content (Join-Path $OutDir 'desktop-result.txt')
    } finally {
        for ($show=0;$show -lt $cursorShows;$show++) { [System.Windows.Forms.Cursor]::Hide() }
        $probe.Dispose(); [DesktopProbe]::SetForegroundWindow($gameWindow) | Out-Null
    }
}
$labels = @('audio-selftest','first-start','existing-install')
if ($ReanimatedApk) { $labels += 'reanimated-hd' }
$labels += 'desktop-integration'
$labels += 'native-controller'
if ($AudioProbeOnly) { $labels = @('audio-selftest','first-start') }
foreach ($label in $labels) {
    $env:DS_WIDTH='1366'; $env:DS_HEIGHT='768'
    if ($label -ne 'desktop-integration') { Remove-Item Env:DS_DESKTOP_PROBE_DIR -ErrorAction SilentlyContinue }
    $env:DS_FULLSCREEN = if ($label -eq 'first-start') { '1' } else { '0' }
    if ($label -eq 'first-start') { Remove-Item Env:DS_FULLSCREEN }
    if ($label -eq 'existing-install') { $env:DS_KEY_HOLDS = '300:302:F11,600:602:F11' } else { Remove-Item Env:DS_KEY_HOLDS -ErrorAction SilentlyContinue }
    if ($label -eq 'audio-selftest') { $env:DS_AUDIO_SELFTEST = '1' }
    if ($label -eq 'desktop-integration') {
        Get-ChildItem Env: | Where-Object { $_.Name -like 'DS_CONTROLLER*' -or $_.Name -in @('DS_NATIVE_PAD_TEST','DS_INPUT_DIAG','DS_SHOT_EVERY','DS_SHOT_DIR') } | ForEach-Object { Remove-Item ('Env:'+$_.Name) }
        $env:DS_WIDTH='960'; $env:DS_HEIGHT='600'; $env:DS_FRAMES='0'
        $env:DS_HD_TEXTURES='0'
        $env:DS_DESKTOP_PROBE_DIR=$evidence.Replace('\','/')
    }
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
    if ($label -eq 'desktop-integration') { $env:DS_SHOT='-1:'+$frame.Replace('\','/') }
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
    $probeError=$null
    if ($label -eq 'desktop-integration') {
        try { Invoke-DesktopProbe $process $evidence } catch {
            $probeError=$_
            $_ | Out-String | Set-Content (Join-Path $evidence 'desktop-probe-error.txt')
        }
        if (!$process.HasExited) { $process.CloseMainWindow() | Out-Null }
    }
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
    if ($probeError) { throw $probeError }
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
    } elseif ($label -eq 'desktop-integration') {
        if ($display[3].Value -ne '0' -or $display[4].Value -ne '960' -or $display[5].Value -ne '600') { throw 'Desktop regression surface is incorrect.' }
    } elseif ($display[3].Value -ne '0' -or $display[4].Value -ne '1366' -or $display[5].Value -ne '768') { throw "$label did not create the expected windowed regression surface." }
    ($display[4].Value + ' ' + $display[5].Value) | Set-Content (Join-Path $evidence ($label + '-frame-size.txt'))
    if ($label -eq 'desktop-integration') {
        '900 540' | Set-Content (Join-Path $evidence ($label+'-frame-size.txt'))
        $pauses=@(Select-String -LiteralPath $stderr -SimpleMatch '[APP_LIFECYCLE] pause')
        if ($pauses.Count -lt 5) { throw 'Real desktop focus-loss events were not observed.' }
        $samples=@(Select-String -LiteralPath $stderr -Pattern '\[CURSOR_PROBE\] serial=(\d+) drawn=1 sample=(\d+),(\d+)')
        if ($samples.Count -ne 3) { throw 'The in-frame cursor was not rendered in all three capture probes.' }
        $names=@('desktop-windowed','desktop-fullscreen','desktop-restored')
        for ($n=0;$n -lt 3;$n++) {
            $groups=$samples[$n].Matches[0].Groups
            python -c "import sys; from PIL import Image; a=Image.open(sys.argv[1]).convert('RGB'); b=Image.open(sys.argv[2]).convert('RGB'); assert a.size==b.size,(a.size,b.size); p=tuple(map(int,sys.argv[3:5])); assert min(a.getpixel(p))>240,('Game cursor missing',p,a.getpixel(p)); assert min(b.getpixel(p))>240,('Desktop cursor missing',p,b.getpixel(p)); assert any(lo!=hi for lo,hi in b.getextrema()),'Blank external capture'; print('PASS: game/desktop capture cursor',a.size,p)" (Join-Path $evidence ("cursor-$($n+1).ppm")) (Join-Path $evidence ($names[$n]+'.png')) $groups[2].Value $groups[3].Value
            if ($LASTEXITCODE -ne 0) { throw 'External desktop capture or cursor pixels did not match.' }
        }
    }
    if (!(Select-String -LiteralPath $stderr -SimpleMatch '[WINDOWS_HOST] console_attached=0')) { throw "$label inherited a console." }
    if (!(Select-String -LiteralPath $stderr -SimpleMatch '[DESKTOP_WINDOW] presentation=composited-borderless sdl_fullscreen=0 auto_minimize=0')) { throw "$label is not using the capture-compatible window path." }
    if ($label -eq 'existing-install') {
        $toggles = @(Select-String -LiteralPath $stderr -Pattern '\[DISPLAY_TOGGLE\] fullscreen=(\d)')
        if ($toggles.Count -ne 2 -or $toggles[0].Matches[0].Groups[1].Value -ne '1' -or $toggles[1].Matches[0].Groups[1].Value -ne '0') { throw 'F11 fullscreen/windowed round trip failed.' }
    }
    if (!(Select-String -LiteralPath $stderr -SimpleMatch '[WINDOWS_ACCEL] initialized stationary logical sensor')) { throw "$label did not initialize the Windows sensor bridge." }
    if (Select-String -LiteralPath $stderr -SimpleMatch 'Windows SEH exception') { throw "$label reported a Windows exception." }
    if (!(Test-Path -LiteralPath $frame)) { throw "$label framebuffer capture is missing." }
    $audioClock = Select-String -LiteralPath $stderr -Pattern '\[AUDIO_CLOCK\].*empty_writes=(\d+)' | Select-Object -Last 1
    if (!$audioClock) { throw "$label audio clock evidence is missing." }
    if ($label -ne 'desktop-integration' -and [int]$audioClock.Matches[0].Groups[1].Value -gt 8) { throw "$label audio repeatedly ran out of queued PCM." }
    $audioRate = Select-String -LiteralPath $stderr -Pattern '\[AUDIO_CLOCK\].*nominal_rate=(\d+) measured_rate=([\d.]+)' | Select-Object -Last 1
    if ($label -ne 'desktop-integration' -and (!$audioRate -or [double]::Parse($audioRate.Matches[0].Groups[2].Value,[System.Globalization.CultureInfo]::InvariantCulture) -lt [int]$audioRate.Matches[0].Groups[1].Value * 0.9)) { throw "$label audio delivery remains below 90 percent of its nominal rate." }
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
