# Synthetic only. Verify the same ZIP APIs used by the GUI installer.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'launcher\Core.ps1')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$temp=Join-Path ([IO.Path]::GetTempPath()) ('ds-xperia-launcher-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($temp)
try {
 $path=Join-Path $temp 'synthetic-hd.apk'
 $z=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Create)
 try {
  for($i=0;$i -lt 222;$i++) {
   $entry=$z.CreateEntry(('assets/texture_hd/texture_{0:D3}.ktx' -f $i))
   $w=$entry.Open()
   try {$bytes=[byte[]]@(1,2,3,4,5);$w.Write($bytes,0,$bytes.Length)}
   finally {$w.Dispose()}
  }
  $ignored=$z.CreateEntry('assets/game/libDeadSpace32.so')
  $w=$ignored.Open();$w.WriteByte(99);$w.Dispose()
 }finally {$z.Dispose()}
 $sum=Get-Sha256File $path
 $zip=Open-CheckedZip $path ([IO.FileInfo]$path).Length @($sum)
 try {
  $items=Test-HdArchive $zip
  if($items.Count -ne 222){throw ('Expected 222 HD KTX files, found '+$items.Count)}
  $staging=Join-Path $temp 'staging'
  [void][IO.Directory]::CreateDirectory($staging)
  Copy-GameMembers $items $staging 'Hd'
  $found=Get-ChildItem -LiteralPath $staging -File -Recurse
  if(@($found).Count -ne 222){throw 'ZIP extraction did not extract exactly 222 HD files'}
  if(Test-Path -LiteralPath (Join-Path $staging 'assets')){throw 'Different-edition game contents leaked into Xperia install'}
  $sample=Join-Path $staging 'enhancements\reanimated\texture_hd\texture_000.ktx'
  if(-not (Test-Path -LiteralPath $sample) -or (Get-Item -LiteralPath $sample).Length -ne 5) {throw 'HD stream extraction corrupted'}
 }finally{$zip.Dispose()}
 $bad=Join-Path $temp 'traversal.apk'
 $z=[IO.Compression.ZipFile]::Open($bad,[IO.Compression.ZipArchiveMode]::Create)
 try {$w=$z.CreateEntry('assets/texture_hd/../escape.ktx').Open();$w.WriteByte(1);$w.Dispose()}
 finally {$z.Dispose()}
 $zip=Open-CheckedZip $bad ([IO.FileInfo]$bad).Length @((Get-Sha256File $bad))
 try {
  $denied=$false
  try {$null=Test-HdArchive $zip}catch{$denied=$true}
  if(-not $denied){throw 'Unsafe HD ZIP traversal was accepted'}
 }finally {$zip.Dispose()}
 Write-Output 'PASS: actual PowerShell installer verifies, extracts and isolates 222 synthetic KTX files; rejected traversal; no copyrighted EA files.'
}finally {if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Recurse -Force}}
