[CmdletBinding()]
param([string]$ProjectDir = "", [switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
try {
    if (!$ProjectDir) {
        $ProjectDir = Join-Path $env:USERPROFILE 'Downloads\dspc_milestone5_one_click_WSL_FIXED\dspc'
    }
    $ProjectDir = [IO.Path]::GetFullPath($ProjectDir)
    foreach ($p in @('tools\out\protos.h','tools\out\dspc_sdl',
                      'apk\lib\armeabi\libDeadSpace.so','apk\assets')) {
        if (!(Test-Path -LiteralPath (Join-Path $ProjectDir $p))) {
            throw "Milestone 5 project incomplete: $p. No changes made."
        }
    }
    $python = Join-Path $PSScriptRoot 'tools\texturepack_fault_guard.py'
    $script = Join-Path $PSScriptRoot 'Apply-TexturePack-Linux.sh'
    if (!(Test-Path -LiteralPath $python) -or !(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'tools\loader_vector_probe.py')) -or !(Test-Path -LiteralPath $script)) {
        throw 'Incomplete diagnostic kit: tools/texturepack_fault_guard.py or Apply-TexturePack-Linux.sh missing.'
    }
    if (!(Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        throw 'WSL unavailable; use the Linux shell script directly in Linux.'
    }
    $rootWSL = @( & wsl.exe --exec wslpath -a -u $ProjectDir )
    if ($LASTEXITCODE -ne 0 -or !$rootWSL) { throw 'Cannot translate game project path to WSL.' }
    $bashWSL = @( & wsl.exe --exec wslpath -a -u $script )
    $pythonWSL = @( & wsl.exe --exec wslpath -a -u $python )
    if ($LASTEXITCODE -ne 0 -or !$bashWSL -or !$pythonWSL) {
        throw 'Cannot translate the scripts to WSL paths.'
    }
    Write-Host '[DS TEXPACK] Preserving original APK, assets, saves and previous Linux executable.'
    & wsl.exe --exec bash ($bashWSL[0].Trim()) ($rootWSL[0].Trim()) ($pythonWSL[0].Trim())
    if ($LASTEXITCODE -ne 0) {
        throw "Diagnostic relink failed. See $ProjectDir\tools\out\texturepack-build.log."
    }
    Write-Host "[DS TEXPACK] Instrumented original Linux native executable: $ProjectDir\tools\out\dspc_sdl"
    Write-Host '[DS TEXPACK] New vector trace: [M3G_LOADER_AFTER_DECODE] then [M3G_LOADER_AFTER_CLEANUP], followed by [TEXPACK_SOURCE_VECTOR].'
    $launcher = Join-Path $ProjectDir 'Play Dead Space (WSL).cmd'
    if (!$NoLaunch -and (Test-Path -LiteralPath $launcher)) {
        # The current WSL launcher inherits an opt-in asset audit, where supported.
        $env:DS_ASSET_TRACE='1'
        if ($env:WSLENV) { if ($env:WSLENV -notmatch '(?:^|:)DS_ASSET_TRACE(?:/|:|$)') { $env:WSLENV += ':DS_ASSET_TRACE/u' } }
        else { $env:WSLENV='DS_ASSET_TRACE/u' }
        Start-Process -FilePath $launcher -WorkingDirectory $ProjectDir
    } else { Write-Host '[DS TEXPACK] Use the original Milestone 5 Play command.' }
    exit 0
} catch {
    Write-Host "[DS TEXPACK ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
