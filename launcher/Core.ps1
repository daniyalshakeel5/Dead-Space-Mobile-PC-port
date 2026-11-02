function Quote-Argument([string]$Value) {
 # Windows CommandLineToArgvW quoting, including trailing backslashes.
 return '"'+[regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1')+'"'
}
function Get-JavaVersion([string]$Path) {
 if(-not (Test-Path -LiteralPath $Path -PathType Leaf)){return $null}
 $p=New-Object Diagnostics.Process
 $p.StartInfo=New-Object Diagnostics.ProcessStartInfo
 $p.StartInfo.FileName=$Path;$p.StartInfo.Arguments='-version'
 $p.StartInfo.UseShellExecute=$false;$p.StartInfo.CreateNoWindow=$true
 $p.StartInfo.RedirectStandardOutput=$true;$p.StartInfo.RedirectStandardError=$true
 try {
  [void]$p.Start();$out=$p.StandardOutput.ReadToEndAsync();$err=$p.StandardError.ReadToEndAsync()
  if(-not $p.WaitForExit(4000)){$p.Kill();return $null}
  $text=$out.Result+$err.Result
  if($p.ExitCode -eq 0 -and $text -match 'version\s+"(?<major>\d+)(?:\.(?<minor>\d+))?'){
   $major=[int]$Matches.major;if($major -eq 1){$major=[int]$Matches.minor}
   if($major -ge 17){return @{Path=$Path;Major=$major}}
  }
 }catch {} finally {$p.Dispose()}
 return $null
}
function Find-Java([string]$Preferred,[string]$RuntimeRoot) {
 $candidates=@();if($Preferred){$candidates+=$Preferred}
 if(Test-Path -LiteralPath $RuntimeRoot){$candidates+=@(Get-ChildItem -LiteralPath $RuntimeRoot -Filter java.exe -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | ForEach-Object {$_.FullName})}
 if($env:JAVA_HOME){$candidates+=(Join-Path $env:JAVA_HOME 'bin\java.exe')}
 $cmd=Get-Command java.exe -ErrorAction SilentlyContinue;if($cmd){$candidates+=$cmd.Source}
 foreach($base in @($env:ProgramFiles,${env:ProgramFiles(x86)})){
  if($base){foreach($vendor in @('Eclipse Adoptium','Java','Microsoft')){
   $dir=Join-Path $base $vendor
   if(Test-Path -LiteralPath $dir){$candidates+=@(Get-ChildItem -LiteralPath $dir -Directory -ErrorAction SilentlyContinue | ForEach-Object {Join-Path $_.FullName 'bin\java.exe'})}
  }}
 }
 foreach($candidate in ($candidates | Select-Object -Unique)){$found=Get-JavaVersion $candidate;if($found){return $found}}
 return $null
}
function Test-AssetCache($Manifest,[string]$Root) {
 foreach($file in $Manifest.resources){
  $path=Join-Path $Root $file.name
  if(-not (Test-Path -LiteralPath $path -PathType Leaf)){return $false}
  if((Get-Item -LiteralPath $path).Length -ne $file.bytes){return $false}
  if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256){return $false}
 }
 return $true
}

# Dead Space user-owned ZIP installation service. Derived from the original
# Diamond Rush launcher Core.ps1's SHA256/local-cache approach and extended
# with bounded ZipArchive extraction; no Java or Python dependency.
$script:XperiaHash = '04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31'
$script:XperiaBytes = 145903794L
$script:HdReanimatedHashes = @(
 '042c0d60c53c72d3c71137710834f2081684625d66d0c3d214d9e75d468830a3',
 'dbd94ee54fc3fefdddb101fac4eba1e2058a5db7cfcf30ac41ad96e88b285a3a')
$script:HdReanimatedBytes = 715366829L
$script:XperiaCriticalHashes = @{
 'lib/armeabi/libEAMGameDeadSpace.so' = '1ff0a34a2ef35f50c0031b383d8fd7b14369dca0f41969c5d4da8fabe569ed38'
 'assets/EAMCore.ini' = 'd5b49184291c9a56928481380e19e8ead7f2df7b031201ffad52d8f374e05335'
 'assets/published/data/level_index.sb' = 'ef14959fea9519b6f698e153bc66cf0a4f9a49bff03d97f0f14b204136320636'
 'assets/published/layoutdata/layouts.xml.sb' = '0f99badf5a7523ba0cc3e5749bf18b40807eb2d0fd11a66cc3c9c65656b4233a'
 'assets/published/models/main_menu.m3g' = '58f88fed2d49276b7fc492ce2286f657820df3ba2b02cc01cda24c13cdd8d9e3'
 'assets/published/models/isaac.m3g' = 'fe287a58c55f7290dda080a0db43995bf0deb4c07c2580a509fafe87f33c3c02'
 'assets/published/textures/texture_isaac_suit_01.m3g' = 'c6e1c92745dfc2f75f6896cdb23a15fdc0f4041af349a25bc169ce3175c3294c'
 'assets/published/texturepacks_ui/ui_frontend.m3g' = '0fa9c375a867dbeba56d34c28d2d4a0ea2cffdeb63869486be3ebc2bf08fa809'
}
function Get-Sha256File([string]$Path) {
 return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Get-Sha256Entry($Entry) {
 $sha=[Security.Cryptography.SHA256]::Create()
 $stream=$Entry.Open()
 try {return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}
 finally {$stream.Dispose();$sha.Dispose()}
}
function Test-SafeArchivePath([string]$Name) {
 if([string]::IsNullOrWhiteSpace($Name) -or $Name.Length -gt 512 -or $Name.Contains('\') -or $Name.Contains([char]0) -or $Name.StartsWith('/')) {throw 'Unsafe ZIP member path.'}
 foreach($part in $Name.Split('/')) {
  if(-not $part -or $part -eq '.' -or $part -eq '..' -or $part.Contains(':') -or $part.EndsWith('.') -or $part.EndsWith(' ')) {throw 'ZIP traversal/Windows path collision rejected.'}
  if($part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') {throw 'ZIP Windows device path rejected.'}
 }
}
function Test-ZipMemberSymlink($Entry) {
 $unixMode=($Entry.ExternalAttributes -shr 16) -band 0xF000
 if($unixMode -eq 0xA000) {throw 'ZIP symlinks are not allowed.'}
}
function Open-CheckedZip([string]$Source,[long]$ExpectedBytes,[string[]]$ExpectedHashes) {
 if(-not (Test-Path -LiteralPath $Source -PathType Leaf)) {throw 'Source ZIP/APK not found.'}
 $size=(Get-Item -LiteralPath $Source).Length
 if($size -ne $ExpectedBytes) {throw 'Source ZIP/APK has an unexpected file size.'}
 $sum=Get-Sha256File $Source
 if($sum -notin $ExpectedHashes) {throw 'Original ZIP/APK SHA-256 does not match the supported release.'}
 Add-Type -AssemblyName System.IO.Compression
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 return [IO.Compression.ZipFile]::OpenRead($Source)
}
function Get-CheckedMembers($Archive,[string]$Prefix,[string]$Mode) {
 $items=New-Object 'System.Collections.Generic.List[object]'
 $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
 $total=0L;$limit=if($Mode -eq 'Hd') {1610612736L} else {367001600L}
 $fileLimit=if($Mode -eq 'Hd'){268435456L}else{33554432L}
 if($Archive.Entries.Count -gt 50000){throw 'Archive member count exceeds safety limit.'}
 foreach($entry in $Archive.Entries) {
  $name=$entry.FullName.Replace('\','/')
  if(-not $name.StartsWith($Prefix,[StringComparison]::Ordinal)) {continue}
  if($entry.FullName.Contains('\')) {throw 'Backslash in game ZIP/APK path.'}
  if($name.EndsWith('/')) {continue}
  $rel=$name.Substring($Prefix.Length)
  Test-SafeArchivePath $rel
  Test-ZipMemberSymlink $entry
  if($Mode -eq 'Original') {
   if(-not $rel.StartsWith('assets/',[StringComparison]::Ordinal) -and $rel -ne 'lib/armeabi/libEAMGameDeadSpace.so') {continue}
  } elseif(-not $rel.ToLowerInvariant().EndsWith('.ktx')) {throw 'Unexpected non-KTX HD file.'}
  if($rel.EndsWith('.DS_Store')){continue}
  if(-not $seen.Add($rel)){throw 'Case-colliding or duplicate ZIP entry.'}
  if($entry.Length -le 0 -or $entry.Length -gt $fileLimit){throw 'Archive member size out of range.'}
  $total+=$entry.Length
  if($total -gt $limit){throw 'Archive uncompressed size exceeds safety limit.'}
  $items.Add([pscustomobject]@{ Entry=$entry; Relative=$rel })
 }
 if($Mode -eq 'Hd'){
  if($items.Count -lt 200 -or $items.Count -gt 320){throw 'Reanimated HD KTX member count is unexpected.'}
 } else {
  $assetCount=@($items | Where-Object {$_.Relative.StartsWith('assets/')}).Count
  if($assetCount -lt 1700 -or $total -lt 254000000L){throw 'Xperia Play campaign archive is incomplete.'}
 }
 return ,$items
}
function Test-OriginalArchive($Archive) {
 $items=Get-CheckedMembers $Archive 'deadspace/' 'Original'
 $byName=@{};foreach($item in $items){$byName[$item.Relative]=$item.Entry}
 foreach($name in $script:XperiaCriticalHashes.Keys) {
  if(-not $byName.ContainsKey($name)){throw ('Required Xperia asset missing: '+$name)}
  if((Get-Sha256Entry $byName[$name]) -ne $script:XperiaCriticalHashes[$name]){throw ('Xperia asset hash mismatch: '+$name)}
 }
 return ,$items
}
function Test-HdArchive($Archive) {
 return ,(Get-CheckedMembers $Archive 'assets/texture_hd/' 'Hd')
}
function Copy-GameMembers($Items,[string]$Root,[string]$Layer) {
 $buffer=New-Object byte[] 1048576
 foreach($item in $Items) {
  $rel=if($Layer -eq 'Hd') {'enhancements/reanimated/texture_hd/'+$item.Relative}else{$item.Relative}
  $out=Join-Path $Root ($rel.Replace('/',[IO.Path]::DirectorySeparatorChar))
  $parent=[IO.Path]::GetDirectoryName($out)
  [void][IO.Directory]::CreateDirectory($parent)
  $source=$item.Entry.Open();$target=$null
  try {
   $target=[IO.File]::Open($out,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
   $copied=0L
   while(($n=$source.Read($buffer,0,$buffer.Length)) -gt 0) {
    $copied+=$n
    if($copied -gt $item.Entry.Length){throw 'ZIP stream expanded unexpectedly.'}
    $target.Write($buffer,0,$n)
   }
   if($copied -ne $item.Entry.Length){throw 'Truncated ZIP extraction.'}
  }finally {if($target){$target.Dispose()};$source.Dispose()}
 }
}
function Install-DeadSpaceAssets([string]$GameZip,[string]$HdApk,[string]$Destination) {
 $gameArchive=$null;$hdArchive=$null;$stage=$null
 try {
  Write-Output 'Checking Xperia Play v1.1.33 archive SHA-256...'
  $gameArchive=Open-CheckedZip $GameZip $script:XperiaBytes @($script:XperiaHash)
  $gameItems=Test-OriginalArchive $gameArchive
  Write-Output ('Xperia campaign verified: '+$gameItems.Count+' members.')
  $hdItems=$null;$hdHash='';
  if($HdApk) {
   Write-Output 'Checking the independent Reanimated v1.2/v1.3 APK SHA-256...'
   $hdArchive=Open-CheckedZip $HdApk $script:HdReanimatedBytes $script:HdReanimatedHashes
   $hdItems=Test-HdArchive $hdArchive
   $hdHash=Get-Sha256File $HdApk
   Write-Output ('Reanimated HD layer verified: '+$hdItems.Count+' KTX files.')
  }
  $dest=[IO.Path]::GetFullPath($Destination)
  if([IO.Directory]::Exists($dest) -or [IO.File]::Exists($dest)){throw 'Install destination already exists; no files overwritten.'}
  $parent=[IO.Path]::GetDirectoryName($dest)
  [void][IO.Directory]::CreateDirectory($parent)
  $stage=Join-Path $parent ('.deadspace-stage-'+[guid]::NewGuid().ToString('N'))
  [void][IO.Directory]::CreateDirectory($stage)
  Write-Output 'Extracting verified Xperia assets into private staging folder...'
  Copy-GameMembers $gameItems $stage 'Original'
  if($hdArchive){Write-Output 'Extracting HD KTX sidecar (original assets remain unchanged)...';Copy-GameMembers $hdItems $stage 'Hd'}
  foreach($key in $script:XperiaCriticalHashes.Keys) {
   $out=Join-Path $stage ($key.Replace('/',[IO.Path]::DirectorySeparatorChar))
   if((Get-Sha256File $out) -ne $script:XperiaCriticalHashes[$key]){throw ('Extracted Xperia file hash mismatch: '+$key)}
  }
  $marker=@{schema=1;profile='xperia-play-v1.1.33-vita-rip-atc';zip_sha256=$script:XperiaHash;hd_apk_sha256=$hdHash;hd_ktx_count=if($hdArchive){$hdItems.Count}else{0};hd_mounted_in_game=$false;runtime_playable=$false}
  $marker | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage '.deadspace-launcher.json') -Encoding UTF8
  if([IO.Directory]::Exists($dest) -or [IO.File]::Exists($dest)){throw 'Install destination appeared; no overwrite.'}
  [IO.Directory]::Move($stage,$dest);$stage=$null
  Write-Output 'SUCCESS: game sources installed. Engine startup is not ready; Play remains disabled.'
 }finally {
  if($hdArchive){$hdArchive.Dispose()};if($gameArchive){$gameArchive.Dispose()}
  if($stage -and [IO.Directory]::Exists($stage)){[IO.Directory]::Delete($stage,$true)}
 }
}
