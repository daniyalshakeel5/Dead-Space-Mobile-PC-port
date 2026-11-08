[CmdletBinding()]
param([string]$ProjectDir = "", [switch]$Quiet)
$ErrorActionPreference = 'Stop'
try {
    if (!$ProjectDir) {
        $ProjectDir = Join-Path $env:USERPROFILE 'Downloads\dspc_milestone5_one_click_WSL_FIXED\dspc'
    }
    $ProjectDir = [IO.Path]::GetFullPath($ProjectDir)
    $generated = Join-Path $ProjectDir 'tools\out'
    $assets = Join-Path $ProjectDir 'apk\assets'
    if (!(Test-Path -LiteralPath $generated -PathType Container) -or
        !(Test-Path -LiteralPath $assets -PathType Container)) {
        throw "Existing Milestone 5 generated C or assets missing at: $ProjectDir. No changes were made."
    }
    $targets = @('f_00365a2c','f_0035c844','f_0035b2e0','f_000adfc4')
    $locations = @{}
    $callers = @{}
    foreach ($name in $targets) { $callers[$name] = New-Object 'System.Collections.Generic.List[string]' }
    $chunks = @(Get-ChildItem -LiteralPath $generated -Filter 'fn_*.c' -File | Sort-Object Name)
    if (!$chunks.Count) { throw 'No translated fn_*.c source files found.' }
    $output = New-Object 'System.Collections.Generic.List[string]'
    [void]$output.Add('Dead Space Mobile - TexturePack original Linux source/data-flow report')
    [void]$output.Add('Owner-private C from original Milestone 5; generated locally, not uploaded to GitHub.')
    [void]$output.Add('Observed native Linux signal: 11, true fault PC: 0x08627456 -> f_0035b2e0.')
    [void]$output.Add('Previously logged: f_00365a2c returned, sp[0..2] = (NULL,NULL,NULL).')
    [void]$output.Add('IMPORTANT: source must establish whether sp is the actual f_00365a2c output vector.')
    [void]$output.Add('')
    foreach ($chunk in $chunks) {
        $lines = [IO.File]::ReadAllLines($chunk.FullName)
        $function = ''
        for ($i = 0; $i -lt $lines.Length; $i++) {
            $line = $lines[$i]
            if ($line -match '^u64 (f_[0-9a-fA-F]{8})\(') {
                $function = $Matches[1]
                if ($targets -contains $function) {
                    if ($locations.ContainsKey($function)) { throw "Duplicate function declaration: $function" }
                    $locations[$function] = @{ file = $chunk.Name; lines = $lines; start = $i }
                }
            }
            foreach ($target in $targets) {
                if ($line.Contains($target + '(') -and !($line.StartsWith('u64 ' + $target + '('))) {
                    if ($callers[$target].Count -lt 20) {
                        $lo = [Math]::Max(0, $i - 25)
                        $hi = [Math]::Min($lines.Length - 1, $i + 17)
                        $segment = New-Object 'System.Collections.Generic.List[string]'
                        [void]$segment.Add(('CALL from {0} {1}:{2}' -f $function, $chunk.Name, ($i+1)))
                        for ($j=$lo; $j -le $hi; $j++) {
                            [void]$segment.Add(('{0,6}: {1}' -f ($j+1), $lines[$j]))
                        }
                        [void]$callers[$target].Add(($segment.ToArray() -join [Environment]::NewLine))
                    }
                }
            }
        }
    }
    foreach ($target in $targets) {
        [void]$output.Add(('========== FUNCTION {0} ==========' -f $target))
        if (!$locations.ContainsKey($target)) {
            [void]$output.Add(('NOT FOUND {0}; check matching original APK/translated source' -f $target))
            continue
        }
        $item = $locations[$target]
        $lines = $item.lines
        $start = [int]$item.start
        $stop = $lines.Length
        for ($j=$start+1; $j -lt $lines.Length; $j++) {
            if ($lines[$j] -match '^u64 f_[0-9a-fA-F]{8}\(') { $stop=$j; break }
        }
        [void]$output.Add(('Source: {0}:{1}; total function lines: {2}' -f $item.file,($start+1),($stop-$start)))
        $limit=[Math]::Min($stop,$start+2000)
        for ($j=$start; $j -lt $limit; $j++) {
            [void]$output.Add(('{0,6}: {1}' -f ($j+1),$lines[$j]))
        }
        if ($limit -lt $stop) {
            [void]$output.Add(('Function exceeds 2000 lines; {0} trailing lines omitted.' -f ($stop-$limit)))
        }
        [void]$output.Add('')
    }
    [void]$output.Add('========== DIRECT CALLER EXCERPTS ==========')
    foreach ($target in $targets) {
        [void]$output.Add(('---- {0} call sites (up to 20) ----' -f $target))
        foreach ($site in $callers[$target]) { [void]$output.Add($site) }
        if (!$callers[$target].Count) { [void]$output.Add('No direct call in fn_*.c (may be indirect dispatch).') }
    }
    [void]$output.Add('')
    [void]$output.Add('========== ORIGINAL APK ASSET LAYOUT (METADATA ONLY) ==========')
    foreach ($rel in @('EAMCore.ini','data.zip','published','published\var','data','var')) {
        $path = Join-Path $assets $rel
        if (Test-Path -LiteralPath $path) {
            $item=Get-Item -LiteralPath $path
            if ($item.PSIsContainer) {
                $entries = @(Get-ChildItem -LiteralPath $path -Force -ErrorAction SilentlyContinue)
                [void]$output.Add(('PRESENT directory {0}; direct entries={1}' -f $rel,$entries.Count))
            } else {
                [void]$output.Add(('PRESENT file {0}; length={1}' -f $rel,$item.Length))
            }
        } else { [void]$output.Add(('MISSING {0}' -f $rel)) }
    }
    $top = @(Get-ChildItem -LiteralPath $assets -Force -ErrorAction Stop | Sort-Object Name | Select-Object -First 150)
    [void]$output.Add('Asset root entries (maximum 150):')
    foreach($item in $top) {
        [void]$output.Add(('{0} {1} {2}' -f $(if($item.PSIsContainer){'DIR '}else{'FILE'}),$item.Name,$(if($item.PSIsContainer){''}else{$item.Length})))
    }
    $report=Join-Path $ProjectDir 'texturepack-decoder-context.txt'
    [IO.File]::WriteAllLines($report,$output.ToArray(),[Text.Encoding]::UTF8)
    Write-Host ('[DS TEXPACK] Generated local source and asset-layout context: {0}' -f $report)
    Write-Host '[DS TEXPACK] No APK assets or source files were modified, rebuilt or uploaded.'
    Write-Host '[DS TEXPACK] Upload texturepack-decoder-context.txt to trace the original output vector.'
    if (!$Quiet) { Start-Process -FilePath notepad.exe -ArgumentList @($report) }
    exit 0
} catch {
    Write-Host ('[DS TEXPACK ERROR] ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
