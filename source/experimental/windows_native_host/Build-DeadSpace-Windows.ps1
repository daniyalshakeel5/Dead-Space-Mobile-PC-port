[CmdletBinding()]
param(
    [string]$ApkPath = "",
    [string]$InstallDir = "",
    [switch]$PrepareOnly,
    [switch]$NoLaunch
)
$ErrorActionPreference = "Stop"
# Compatibility entry point: the old builder used to require an existing
# project. The new default does a full APK-based fresh install.
$installer = Join-Path $PSScriptRoot "Install-DeadSpace-Fresh.ps1"
if (!(Test-Path -LiteralPath $installer)) {
    Write-Error "Missing Install-DeadSpace-Fresh.ps1. Re-extract the full kit."
    exit 1
}
& $installer -ApkPath $ApkPath -InstallDir $InstallDir -PrepareOnly:$PrepareOnly -NoLaunch:$NoLaunch
exit $LASTEXITCODE
