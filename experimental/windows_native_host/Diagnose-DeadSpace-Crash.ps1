[CmdletBinding()]
param([string]$InstallDir = "", [switch]$Quiet)
$ErrorActionPreference = 'Stop'
try {
    if (!$InstallDir) {
        $docs = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
        if (!$docs) { $docs = Join-Path $env:USERPROFILE 'Documents' }
        $InstallDir = Join-Path $docs 'DeadSpaceMobilePC'
    }
    $InstallDir = [IO.Path]::GetFullPath($InstallDir)
    $work = Join-Path $InstallDir 'windows-build'
    $exe = Join-Path $work 'DeadSpaceMobile-Win32.exe'
    $log = Join-Path $work 'logs\windows-run.log'
    $report = Join-Path $work 'logs\crash-symbolized.log'
    if (!(Test-Path -LiteralPath $log -PathType Leaf)) {
        throw "No run log found: $log"
    }
    if (!(Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw "Your private Windows executable is missing: $exe"
    }
    $raw = [IO.File]::ReadAllText($log)
    $hits = [regex]::Matches($raw, 'Windows SEH exception\s+0x([0-9a-fA-F]+)\s+pc=([0-9a-fA-F]+)')
    if ($hits.Count -eq 0) {
        $body = @('No Windows SEH fault address in the log.',
                  'This could be an intentional exit, a trap, or a different failure.',
                  'Inspect windows-run.log for the final engine messages.')
        Set-Content -LiteralPath $report -Value $body -Encoding UTF8
        Write-Host "[DS DIAG] No SEH exception recorded; report: $report"
        if (!$Quiet) { Start-Process notepad.exe -ArgumentList @($report) }
        exit 0
    }
    $match = $hits[$hits.Count - 1]
    $faultCode = $match.Groups[1].Value
    $pc = [Convert]::ToUInt64($match.Groups[2].Value, 16)

    # Read PE32 preferred base from the actual locally compiled image; never
    # assume it is 0x00400000 if link flags ever change.
    $stream = [IO.File]::OpenRead($exe)
    try {
        $br = New-Object IO.BinaryReader($stream)
        $stream.Position = 0x3c
        $pe = $br.ReadUInt32()
        $stream.Position = $pe
        if ($br.ReadUInt32() -ne 0x00004550) { throw "Not a PE image: $exe" }
        $stream.Position = $pe + 24
        $magic = $br.ReadUInt16()
        if ($magic -ne 0x10b) { throw "Expected native 32-bit PE32 (magic $magic)" }
        $stream.Position = $pe + 24 + 28
        $preferred = [UInt64]$br.ReadUInt32()
    } finally { $stream.Dispose() }

    $bases = [regex]::Matches($raw, 'process image base=(?:0x)?([0-9a-fA-F]+)')
    $imageBase = $preferred
    $warning = ''
    if ($bases.Count -gt 0) {
        $imageBase = [Convert]::ToUInt64($bases[$bases.Count - 1].Groups[1].Value, 16)
    } else {
        $warning = 'WARNING: Older host log has no loaded module base. Assuming it equals PE preferred base; ASLR may change the address.'
    }
    if ($pc -lt $imageBase) { throw "Fault PC is below module base; check log and executable identity." }
    $rva = $pc - $imageBase
    $linkAddress = $preferred + $rva

    $lines = New-Object 'System.Collections.Generic.List[string]'
    [void]$lines.Add('Dead Space Mobile native i686 crash symbolication')
    [void]$lines.Add(('Exception code: 0x{0}' -f $faultCode))
    [void]$lines.Add(('Runtime PC: 0x{0:X8}' -f $pc))
    [void]$lines.Add(('Loaded image base: 0x{0:X8}' -f $imageBase))
    [void]$lines.Add(('PE preferred image base: 0x{0:X8}' -f $preferred))
    [void]$lines.Add(('Module RVA: 0x{0:X8}' -f $rva))
    [void]$lines.Add(('Address for addr2line: 0x{0:X8}' -f $linkAddress))
    if ($warning) { [void]$lines.Add($warning) }
    [void]$lines.Add('')

    if (!(Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        [void]$lines.Add('WSL unavailable. Cannot map PC to C function. Your executable and log are preserved.')
    } else {
        $wslExe = & wsl.exe --exec wslpath -a -u $exe
        if ($LASTEXITCODE -ne 0 -or !$wslExe) {
            [void]$lines.Add('WSL cannot access the private executable; line symbolication unavailable.')
        } else {
            $wslExe = ($wslExe | Select-Object -First 1).Trim()
            $addr = '0x{0:X8}' -f $linkAddress
            $details = & wsl.exe --exec i686-w64-mingw32-addr2line -a -f -C -i -e $wslExe $addr 2>&1
            [void]$lines.Add(('C source/function for {0}:' -f $addr))
            if ($LASTEXITCODE -ne 0) {
                [void]$lines.Add('addr2line exited unsuccessfully. Ensure WSL binutils-mingw-w64-i686 is installed.')
            }
            foreach ($part in $details) { [void]$lines.Add([string]$part) }
        }
    }
    # The executable and translated sources are private to this machine.
    # Include only a small window from the mapped generated C function.
    # This provides the actual dereference (not just the function name).
    $sources = [regex]::Matches(($details -join "\n"), '([^\r\n]+[/\\]fn_[0-9]+\.c):(\d+)')
    if ($sources.Count -gt 0) {
        $src = $sources[0].Groups[1].Value
        $srcLine = [int]$sources[0].Groups[2].Value
        $baseName = [IO.Path]::GetFileName($src)
        $localPath = Join-Path $InstallDir ('tools\out\' + $baseName)
        if ((Test-Path -LiteralPath $localPath -PathType Leaf) -and $srcLine -gt 0) {
            [void]$lines.Add('')
            [void]$lines.Add(('Translated guest code around {0}:{1}:' -f $baseName, $srcLine))
            $srcText = [IO.File]::ReadAllLines($localPath)
            $lo = [Math]::Max(1, $srcLine - 20)
            $hi = [Math]::Min($srcText.Length, $srcLine + 20)
            for ($i = $lo; $i -le $hi; $i++) {
                $marker = if ($i -eq $srcLine) { '>>' } else { '  ' }
                [void]$lines.Add(('{0} {1,6}: {2}' -f $marker, $i, $srcText[$i - 1]))
            }
        } else {
            [void]$lines.Add("Translated guest source not found: $localPath")
        }
    }
    [void]$lines.Add('')
    [void]$lines.Add('Last 24 run-log lines:')
    $tail = @($raw -split "\r?\n" | Select-Object -Last 24)
    foreach ($line in $tail) { [void]$lines.Add($line) }
    Set-Content -LiteralPath $report -Value $lines -Encoding UTF8
    Write-Host "[DS DIAG] Saved symbolized crash report: $report"
    foreach ($line in $lines) { Write-Host $line }
    if (!$Quiet) { Start-Process notepad.exe -ArgumentList @($report) }
    exit 0
} catch {
    Write-Host "[DS DIAG ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
