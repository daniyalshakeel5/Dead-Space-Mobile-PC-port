param(
 [Parameter(Mandatory=$true)][string]$GameDir,
 [string]$ToolPath=(Join-Path $PSScriptRoot 'xperia_guest_import_report.exe'),
 [string]$ReportPath=''
)
$ErrorActionPreference='Stop'
try {
 $root=[IO.Path]::GetFullPath($GameDir)
 $original=Join-Path $root 'lib\armeabi\libEAMGameDeadSpace.so'
 if(-not (Test-Path -LiteralPath $original -PathType Leaf)) {
  throw 'Original Xperia Play ARM game library not installed.'
 }
 if(-not (Test-Path -LiteralPath $ToolPath -PathType Leaf)) {
  throw 'Windows ARM guest import scanner missing. Install the matching runtime artifact.'
 }
 if((Get-Item -LiteralPath $original).Length -ne 4499468) {
  throw 'Unrecognized Xperia ARM game library. Refusing mismatched guest code.'
 }
 if(-not $ReportPath) {
  $ReportPath=Join-Path $env:LOCALAPPDATA 'DeadSpaceMobile-Xperia\launcher-logs\guest-preflight.json'
 }
 $dest=[IO.Path]::GetFullPath($ReportPath)
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest)) | Out-Null
 # Never carry a stale success forward.
 if(Test-Path -LiteralPath $dest){Remove-Item -LiteralPath $dest -Force}
 $stderr=[IO.Path]::GetTempFileName()
 try {
  & $ToolPath $original 2> $stderr | Out-File -LiteralPath $dest -Encoding utf8
  $code=$LASTEXITCODE
  $message=Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue
  if($code -notin @(0,4)) {
   throw ('Guest parser rejected original game. '+$message)
  }
  $report=Get-Content -LiteralPath $dest -Raw | ConvertFrom-Json
  if($report.schema -ne 1 -or $report.input -ne 'locally-installed-Xperia-ARM32') {
   throw 'Unexpected guest report schema.'
  }
  Write-Output ('ELF imported; guest initializers: '+$report.initializers)
  Write-Output ('External relocation sites: '+$report.pending_external_relocations)
  Write-Output ('Missing runtime symbols: '+$report.required_unimplemented_symbols)
  Write-Output ('Detailed report: '+$dest)
  if($code -eq 4) {
   Write-Output 'BLOCKED: Android/JNI runtime imports remain. This is not a successful game boot.'
  } else {
   Write-Output 'All reported imports resolved; gameplay execution is still unverified.'
  }
  exit $code
 }finally {Remove-Item -LiteralPath $stderr -Force -ErrorAction SilentlyContinue}
}catch {
 Write-Error ('XPERIA_WINDOWS_PREFLIGHT_FAILED: '+$_.Exception.Message)
 exit 2
}
