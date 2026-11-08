[CmdletBinding()]
param([string]$InstallDir = "")
$ErrorActionPreference = 'Stop'
try {
    if (!$InstallDir) {
        $docs = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
        if (!$docs) { $docs = Join-Path $env:USERPROFILE 'Documents' }
        $InstallDir = Join-Path $docs 'DeadSpaceMobilePC'
    }
    $root = [IO.Path]::GetFullPath($InstallDir)
    $generated = Join-Path $root 'tools\out'
    if (!(Test-Path -LiteralPath $generated -PathType Container)) {
        throw "Private translated C directory not found: $generated"
    }
    $logDir = Join-Path $root 'windows-build\logs'
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    $report = Join-Path $logDir 'modelcache-source-context.txt'
    $summary = New-Object 'System.Collections.Generic.List[string]'
    [void]$summary.Add('Dead Space Mobile — local ModelCache crash source context')
    [void]$summary.Add('Crash: f_000629b4, loadNodeUncached, dereference at fn_003.c:17449')
    [void]$summary.Add('Observed: r3 = LD32(sp + 52u) was zero before LD32(r3).')
    [void]$summary.Add('No game source, APK, or original assets have been uploaded to GitHub.')
    [void]$summary.Add('')

    function Add-FunctionExcerpt([string]$Symbol, [int]$MaxLines) {
        $needle = 'u64 ' + $Symbol + '('
        $matchFile = $null
        $lines = $null
        $start = -1
        # Deterministically restrict the primary function to its known chunk.
        $candidates = if ($Symbol -eq 'f_000629b4') {
            @(Join-Path $generated 'fn_003.c')
        } else {
            @(Get-ChildItem -LiteralPath $generated -Filter 'fn_*.c' -File | Select-Object -ExpandProperty FullName)
        }
        foreach ($file in $candidates) {
            if (!(Test-Path -LiteralPath $file)) { continue }
            $buf = [IO.File]::ReadAllLines($file)
            for ($i = 0; $i -lt $buf.Length; $i++) {
                if ($buf[$i].StartsWith($needle, [StringComparison]::Ordinal)) {
                    if ($matchFile) { throw "Symbol $Symbol appears more than once" }
                    $matchFile = $file
                    $lines = $buf
                    $start = $i
                }
            }
            if ($matchFile) { break }
        }
        if (!$matchFile -or $start -lt 0) {
            [void]$summary.Add("SOURCE UNAVAILABLE: $Symbol")
            return
        }
        $stop = $lines.Length
        for ($i = $start + 1; $i -lt $lines.Length; $i++) {
            if ($lines[$i] -match '^u64 f_[0-9a-fA-F]{8}\(') {
                $stop = $i
                break
            }
        }
        $count = $stop - $start
        [void]$summary.Add(('FUNCTION {0} at {1}:{2} ({3} lines)' -f $Symbol, [IO.Path]::GetFileName($matchFile), ($start+1), $count))
        [void]$summary.Add('---')
        $limit = [Math]::Min($stop, $start + $MaxLines)
        for ($i = $start; $i -lt $limit; $i++) {
            $marker = if ($lines[$i] -match 'r7\s*=\s*LD32\(ea\)' -and $i+1 -ge 17439 -and $i+1 -le 17460) { '>>' } else { '  ' }
            [void]$summary.Add(('{0}{1,6}: {2}' -f $marker, ($i+1), $lines[$i]))
        }
        if ($limit -lt $stop) { [void]$summary.Add(('TRUNCATED: {0} further source lines' -f ($stop-$limit))) }
        [void]$summary.Add('')
        [void]$summary.Add('Stack slot mentions (C source, not necessarily dynamically executed):')
        for ($i = $start; $i -lt $stop; $i++) {
            if ($lines[$i] -match '\bsp\s*[+\-]\s*52u\b' -or
                $lines[$i] -match 'ST32\(' -or
                $lines[$i] -match 'f_0027e644\(') {
                # Only target instructions in the primary function for compact output.
                if ($Symbol -eq 'f_000629b4') {
                    [void]$summary.Add(('{0,6}: {1}' -f ($i+1), $lines[$i]))
                }
            }
        }
        [void]$summary.Add('')
    }

    Add-FunctionExcerpt -Symbol 'f_000629b4' -MaxLines 1100
    Add-FunctionExcerpt -Symbol 'f_0027e644' -MaxLines 700
    Set-Content -LiteralPath $report -Value $summary -Encoding UTF8
    Write-Host "[DS MODEL CACHE] Wrote source context: $report"
    Write-Host "[DS MODEL CACHE] Upload modelcache-source-context.txt to continue the native-code fix."
    Write-Host '[DS MODEL CACHE] The report contains only the targeted translated functions, not the APK.'
    if (!$env:DS_DIAG_QUIET) {
        Start-Process -FilePath notepad.exe -ArgumentList @($report)
    }
    exit 0
} catch {
    Write-Host "[DS MODEL CACHE ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
