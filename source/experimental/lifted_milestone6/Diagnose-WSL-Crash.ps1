[CmdletBinding()]
param([string]$LogPath = "", [switch]$ParseOnly, [switch]$Quiet)
$ErrorActionPreference = 'Stop'
try {
    if (!$LogPath) {
        $candidates = @()
        foreach ($folder in @((Join-Path $env:USERPROFILE 'Downloads'), $PSScriptRoot)) {
            if (Test-Path -LiteralPath $folder -PathType Container) {
                $candidates += @(Get-ChildItem -LiteralPath $folder -Filter 'wsl-game-*.log' -File -ErrorAction SilentlyContinue)
            }
        }
        if (!$candidates.Count) { throw 'No wsl-game-*.log in Downloads or beside the tool. Supply -LogPath or copy the log next to this script.' }
        $LogPath = ($candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
    }
    $LogPath = [IO.Path]::GetFullPath($LogPath)
    if (!(Test-Path -LiteralPath $LogPath -PathType Leaf)) { throw "Log file not found: $LogPath" }
    $raw = [IO.File]::ReadAllText($LogPath)
    $pcHits = [regex]::Matches($raw, '(?im)!! signal ([0-9]+) at pc=(0x[0-9a-fA-F]+), fault address ([^\r\n]+)')
    if (!$pcHits.Count) { throw 'No Linux signal + fault PC found in this log.' }
    $fault = $pcHits[$pcHits.Count - 1]
    $pc = $fault.Groups[2].Value
    $signal = $fault.Groups[1].Value
    $address = $fault.Groups[3].Value.Trim()
    $elfs = [regex]::Matches($raw, '(?im)^loaded\s+(.+?)/apk/lib/armeabi/libDeadSpace[.]so\s+at\s+(0x[0-9a-fA-F]+)')
    if (!$elfs.Count) { throw 'Cannot identify the original WSL game folder from the loaded ELF path.' }
    $root = $elfs[$elfs.Count - 1].Groups[1].Value.Trim()
    $frames = [regex]::Matches($raw, '(?im)^backtrace [(]use nm -n dspc_native to map[)]:([^\r\n]+)')
    $backtrace = @()
    if ($frames.Count) {
        $backtrace = @([regex]::Matches($frames[$frames.Count - 1].Groups[1].Value, '0x[0-9a-fA-F]+') |
            ForEach-Object { $_.Value })
    }
    $report = [IO.Path]::ChangeExtension($LogPath, '.symbolized.txt')
    $lines = New-Object 'System.Collections.Generic.List[string]'
    [void]$lines.Add('Dead Space Mobile - original native Linux i686 crash mapping')
    [void]$lines.Add("Log: $LogPath")
    [void]$lines.Add("Signal: $signal")
    [void]$lines.Add("Fault PC: $pc")
    [void]$lines.Add("Fault address: $address")
    [void]$lines.Add("Game folder (Linux path): $root")
    [void]$lines.Add("Captured backtrace frames: $($backtrace.Count)")
    [void]$lines.Add('Note: backtrace frame 0 is the signal handler; the fault PC above is the actual crash instruction.')
    [void]$lines.Add('')
    if ($raw.Contains('data.zip) -> FileNotFoundException')) {
        [void]$lines.Add('Observation: AssetManager.open(data.zip) failed. This is not yet proven to cause the crash.')
    }
    if ($raw.Contains('GL_RENDERER=llvmpipe')) { [void]$lines.Add('Observation: OpenGL is using software rasterization (llvmpipe).') }
    if ($raw.Contains('com/eamobile/Query.getTotalMemory')) {
        [void]$lines.Add('Observation: Log ends around Query.getTotalMemory; causation unverified.')
    }
    [void]$lines.Add('')
    if ($ParseOnly) {
        [void]$lines.Add('ParseOnly: no WSL invocation; no executable symbols resolved.')
    } elseif (!(Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        [void]$lines.Add('WSL not installed: cannot symbolize native binary.')
    } else {
        # The folder is passed as a positional argument, not shell interpolation.
        $discover = @'
root="$1"
for file in \
  "$root/tools/out/dspc_sdl" \
  "$root/tools/out/dspc_native" \
  "$root/tools/dspc_sdl" \
  "$root/tools/dspc_native" \
  "$root/out/dspc_sdl" \
  "$root/out/dspc_native" \
  "$root/dspc_sdl" \
  "$root/dspc_native"; do
  if test -f "$file"; then printf '%s\n' "$file"; exit 0; fi
done
find "$root" -maxdepth 5 -type f \( -name dspc_sdl -o -name dspc_native \) -print -quit 2>/dev/null
'@
        $found = @(& wsl.exe --exec bash -c $discover _ $root 2>&1)
        if ($LASTEXITCODE -ne 0 -or !$found) {
            [void]$lines.Add("No compiled dspc_sdl/dspc_native binary found beneath $root")
        } else {
            $binary = ([string]($found | Select-Object -First 1)).Trim()
            [void]$lines.Add("Native ELF32 binary: $binary")
            [void]$lines.Add('---- Fault PC and all captured return addresses (addr2line) ----')
            $addresses = @($pc) + $backtrace
            $symbols = @(& wsl.exe --exec addr2line -a -f -C -i -e $binary @addresses 2>&1)
            if ($LASTEXITCODE -ne 0) {
                [void]$lines.Add("addr2line exited $LASTEXITCODE. Install WSL binutils and preserve a debug build.")
            }
            foreach ($line in $symbols) { [void]$lines.Add([string]$line) }
            $sources = [regex]::Matches(($symbols -join [Environment]::NewLine), '([^\r\n]+/fn_[0-9]+[.]c):([0-9]+)')
            $seen = @{}
            $shown = 0
            foreach ($m in $sources) {
                $source = $m.Groups[1].Value
                $sourceLine = [int]$m.Groups[2].Value
                $key = ('{0}:{1}' -f $source, $sourceLine)
                if ($seen.ContainsKey($key) -or $shown -ge 3) { continue }
                $seen[$key] = $true
                $shown++
                [void]$lines.Add('')
                [void]$lines.Add(('Generated C excerpt: {0}:{1}' -f $source, $sourceLine))
                $begin = [Math]::Max(1, $sourceLine - 16)
                $end = $sourceLine + 16
                $snippet = @(& wsl.exe --exec sed -n ("{0},{1}p" -f $begin, $end) $source 2>&1)
                foreach ($line in $snippet) { [void]$lines.Add([string]$line) }
            }
        }
    }
    [IO.File]::WriteAllLines($report, $lines.ToArray(), [Text.Encoding]::UTF8)
    Write-Host "[DS LINUX DIAG] Report saved: $report"
    if (!$Quiet) { Start-Process -FilePath notepad.exe -ArgumentList @($report) }
    exit 0
} catch {
    Write-Host "[DS LINUX DIAG ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
