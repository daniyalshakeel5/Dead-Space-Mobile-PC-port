param([Parameter(Mandatory=$true)][string]$GameZip,
      [string]$HdApk,
      [Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Core.ps1')
try {
 Install-DeadSpaceAssets -GameZip $GameZip -HdApk $HdApk -Destination $Destination
 exit 0
}catch {
 Write-Output ('ERROR: '+$_.Exception.Message)
 exit 1
}
