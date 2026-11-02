# Dead Space Mobile - Windows WPF launcher.
# Adapted from the user's original Diamond Rush N73 v0.7.0 launcher.
# Windows PowerShell 5.1; no Java/Python/Qt required to launch or import assets.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName @('PresentationFramework','PresentationCore','WindowsBase')
. (Join-Path $PSScriptRoot 'Core.ps1')
$data=Join-Path $env:LOCALAPPDATA 'DeadSpaceMobile-Xperia'
$logs=Join-Path $data 'launcher-logs'
$qemuLogs=Join-Path $data 'qemu-boot'
$install=Join-Path $data 'installed'
$config=Join-Path $data 'launcher.json'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
[xml]$xaml=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Launcher.xaml') -Raw -Encoding UTF8
$window=[Windows.Markup.XamlReader]::Load((New-Object Xml.XmlNodeReader $xaml))
$ui=@{}
$xaml.SelectNodes('//*[@Name]')|ForEach-Object {$ui[$_.Name]=$window.FindName($_.Name)}
$script:task=$null;$script:history='';$script:finished='';$script:showSelectHd=$true
try {
 if(Test-Path -LiteralPath $config) {
  $prefs=Get-Content -LiteralPath $config -Raw|ConvertFrom-Json
  $ui.GamePath.Text=[string]$prefs.gameZip
  $ui.HdPath.Text=[string]$prefs.reanimatedApk
  $ui.EnableHd.IsChecked=[bool]$prefs.preferHd
  $ui.Quality.SelectedIndex=[Math]::Min(2,[Math]::Max(0,[int]$prefs.quality))
 }
}catch {$script:history+='Previous settings could not be read; defaults restored.'+"`r`n"}
function Write-Activity([string]$Message) {
 $script:history+='['+(Get-Date -Format 'HH:mm:ss')+'] '+$Message+"`r`n"
 if($script:history.Length -gt 32000){$script:history=$script:history.Substring($script:history.Length-32000)}
}
function Save-Settings {
 try {
  @{gameZip=$ui.GamePath.Text;reanimatedApk=$ui.HdPath.Text;preferHd=[bool]$ui.EnableHd.IsChecked;quality=$ui.Quality.SelectedIndex} |
    ConvertTo-Json | Set-Content -LiteralPath $config -Encoding UTF8
 }catch {Write-Activity ('Failed to save settings: '+$_.Exception.Message)}
}
function Test-Installed {
 $marker=Join-Path $install '.deadspace-launcher.json'
 if(-not (Test-Path -LiteralPath $marker -PathType Leaf)){return $false}
 try {
  $m=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
  return ($m.profile -eq 'xperia-play-v1.1.33-vita-rip-atc' -and
    $m.zip_sha256 -eq $script:XperiaHash -and
    (Test-Path -LiteralPath (Join-Path $install 'lib\armeabi\libEAMGameDeadSpace.so') -PathType Leaf) -and
    (Test-Path -LiteralPath (Join-Path $install 'assets\published\models\main_menu.m3g') -PathType Leaf))
 }catch {return $false}
}
function Refresh-Setup {
 $isInstalled=Test-Installed
 $hasGame= $ui.GamePath.Text -and (Test-Path -LiteralPath $ui.GamePath.Text -PathType Leaf)
 $hasHd= $ui.HdPath.Text -and (Test-Path -LiteralPath $ui.HdPath.Text -PathType Leaf)
 $ui.OriginalStatus.Text=if($isInstalled){'ORIGINAL  //  Installed: Xperia v1.1.33 campaign ready'}elseif($hasGame){'ORIGINAL  //  ZIP selected; import required'}else{'ORIGINAL  //  Select the Vita-ready deadspace.zip'}
 $hdRoot=Join-Path $install 'enhancements\reanimated\texture_hd'
 $hdInstalled=Test-Path -LiteralPath $hdRoot -PathType Container
 $ui.HdStatus.Text=if($hdInstalled){'REANIMATED  //  HD KTX sidecar installed (not mounted by engine)'}elseif($hasHd){'REANIMATED  //  APK selected; import required'}else{'REANIMATED  //  Optional HD APK not selected'}
 $ui.HdDetails.Text=if($hdInstalled){'HD files detected. Original game files remain separate.'}else{'HD files not installed. They will be kept separate from the original game.'}
 # This public Windows launcher must NEVER demand WSL2, Docker,
 # Git or Python merely to play. A self-contained Windows QEMU full-
 # system runtime has not been validated and is not bundled yet.
 $ui.EngineStatus.Text='ENGINE  //  Standalone QEMU Windows runtime not packaged yet.'
 $ui.RuntimeStatus.Text='QEMU DEVELOPMENT BACKEND // Linux qemu-arm works without WSL. Windows packaging remains unverified.'
 $ui.QemuStatus.Text='No WSL2 or Docker requirement for players. Play will unlock only with a verified self-contained PC runtime.'
 $busy=$null -ne $script:task
 $ui.InstallAssets.IsEnabled= -not $busy -and -not $isInstalled
 $ui.QemuBoot.IsEnabled=$false # no WSL/Docker requirement in the player build
 foreach($name in @('BrowseZip','BrowseApk','ClearApk','Recheck','BrowseApkGraphics')){$ui[$name].IsEnabled= -not $busy}
 $ui.OpenOriginal.IsEnabled=$isInstalled
 $ui.OpenHd.IsEnabled=$hdInstalled
 $ui.OpenInstall.IsEnabled=$isInstalled
 $ui.Launch.IsEnabled=$false # Fail closed until real Xperia runtime is wired.
 $ui.EnableHd.IsEnabled=$hdInstalled
 if(-not $busy){$ui.RunStatus.Text=if($isInstalled){'Assets installed. Play is disabled until the ARM runtime boots.'}else{'Select game files and click Install / Verify Game Files.'}}
}
function Show-Page([string]$Name) {
 foreach($part in @('Home','Graphics','Controls','Deps','About')){
  $ui[$part+'Page'].Visibility=if($part -eq $Name){'Visible'}else{'Collapsed'}
  $ui[$part+'Nav'].Background=if($part -eq $Name){'#743439'}else{'#203138'}
 }
}
function Safe-Action([scriptblock]$Action) {
 try{& $Action}catch{
  Write-Activity ('ERROR: '+$_.Exception.Message)
  [void][Windows.MessageBox]::Show($_.Exception.Message,'Dead Space Launcher','OK','Error')
  Refresh-Setup
 }
}
function Choose-Game {
 $pick=New-Object Microsoft.Win32.OpenFileDialog
 $pick.Filter='Xperia Play game ZIP (*.zip)|*.zip|All files (*.*)|*.*'
 $pick.Title='Select original Dead Space Mobile Xperia Play deadspace.zip'
 if($pick.ShowDialog($window)){$ui.GamePath.Text=$pick.FileName;Save-Settings;Write-Activity 'Xperia Play game ZIP selected.';Refresh-Setup}
}
function Choose-HD {
 $pick=New-Object Microsoft.Win32.OpenFileDialog
 $pick.Filter='Reanimated Android package (*.apk)|*.apk|All files (*.*)|*.*'
 $pick.Title='Select Dead Space Reanimated APK for HD KTX textures'
 if($pick.ShowDialog($window)){$ui.HdPath.Text=$pick.FileName;Save-Settings;Write-Activity 'Reanimated HD APK selected.';Refresh-Setup}
}
function Start-Tracked([string]$Executable,[string[]]$Arguments,[string]$Kind='Asset installation') {
 $stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6)
 $stdout=Join-Path $logs ($stamp+'.out.log');$stderr=Join-Path $logs ($stamp+'.err.log')
 $argsText=($Arguments | ForEach-Object {Quote-Argument $_}) -join ' '
 $p=Start-Process -FilePath $Executable -ArgumentList $argsText -WorkingDirectory $PSScriptRoot -RedirectStandardOutput $stdout -RedirectStandardError $stderr -WindowStyle Hidden -PassThru
 $null=$p.Handle
 return @{Process=$p;Out=$stdout;Err=$stderr;Kind=$Kind}
}
function Read-LogTail([string]$Path) {
 try {
  $s=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
  try{
   if($s.Length -gt 24000){[void]$s.Seek(-24000,[IO.SeekOrigin]::End)}
   $rd=New-Object IO.StreamReader $s
   try{return $rd.ReadToEnd()}finally{$rd.Dispose()}
  }finally{$s.Dispose()}
 }catch{return ''}
}
$ui.HomeNav.Add_Click({Show-Page 'Home'})
$ui.GraphicsNav.Add_Click({Show-Page 'Graphics'})
$ui.ControlsNav.Add_Click({Show-Page 'Controls'})
$ui.DepsNav.Add_Click({Show-Page 'Deps'})
$ui.AboutNav.Add_Click({Show-Page 'About'})
$ui.BrowseZip.Add_Click({Safe-Action {Choose-Game}})
$ui.BrowseApk.Add_Click({Safe-Action {Choose-HD}})
$ui.BrowseApkGraphics.Add_Click({Safe-Action {Choose-HD}})
$ui.ClearApk.Add_Click({$ui.HdPath.Text='';$ui.EnableHd.IsChecked=$false;Save-Settings;Refresh-Setup;Write-Activity 'Original ATC textures selected; no HD APK.'})
$ui.Recheck.Add_Click({Refresh-Setup;Write-Activity 'Setup status refreshed.'})
$ui.OpenOriginal.Add_Click({Safe-Action {Start-Process explorer.exe -ArgumentList (Quote-Argument (Join-Path $install 'assets'))}})
$ui.OpenHd.Add_Click({Safe-Action {Start-Process explorer.exe -ArgumentList (Quote-Argument (Join-Path $install 'enhancements\reanimated\texture_hd'))}})
$ui.OpenInstall.Add_Click({Safe-Action {Start-Process explorer.exe -ArgumentList (Quote-Argument $install)}})
$ui.OpenLogs.Add_Click({Start-Process explorer.exe -ArgumentList (Quote-Argument $logs)})
$ui.OpenBootLogs.Add_Click({Safe-Action {
 if(-not (Test-Path -LiteralPath $qemuLogs -PathType Container)){
  throw 'No QEMU boot logs yet. Run the boot check after installing the original Xperia ZIP.'
 }
 Start-Process explorer.exe -ArgumentList (Quote-Argument $qemuLogs)
}})
$ui.CopyLog.Add_Click({[Windows.Clipboard]::SetText($ui.Log.Text)})
$ui.OpenProject.Add_Click({Start-Process 'https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/tree/work/deadspace-xperia-arm-wrapper-2026-11-02'})
$ui.EnableHd.Add_Click({Save-Settings})
$ui.Quality.Add_SelectionChanged({Save-Settings})
$ui.InstallAssets.Add_Click({Safe-Action {
 if(-not $ui.GamePath.Text -or -not (Test-Path -LiteralPath $ui.GamePath.Text -PathType Leaf)){Choose-Game}
 if(-not $ui.GamePath.Text){return}
 if(-not $ui.HdPath.Text){
  $q=[Windows.MessageBox]::Show('Would you like to import the HD texture pack from your Dead Space Reanimated APK? This is optional; it stays separate from the original Xperia game.','Optional Reanimated HD textures','YesNo','Question')
  if($q -eq 'Yes'){Choose-HD}
 }
 $argsList=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Installer.ps1'),'-GameZip',$ui.GamePath.Text,'-Destination',$install)
 if($ui.HdPath.Text){
  if(-not (Test-Path -LiteralPath $ui.HdPath.Text -PathType Leaf)){throw 'The selected Reanimated APK no longer exists.'}
  $argsList+=@('-HdApk',$ui.HdPath.Text)
 }
 Save-Settings
 $script:task=Start-Tracked (Join-Path $PSHOME 'powershell.exe') $argsList
 $ui.RunStatus.Text='Verifying both archives, then extracting original and HD content...'
 Write-Activity 'Private install started. Your original files remain on this PC.'
 Refresh-Setup
}})
# Match Diamond Rush v0.7.0's nonblocking log/status timer.
$timer=New-Object Windows.Threading.DispatcherTimer
$timer.Interval=[TimeSpan]::FromMilliseconds(500)
$timer.Add_Tick({
 $text=$script:history
 if($script:task){
  $item=$script:task
  $out=(Read-LogTail $item.Out)+(Read-LogTail $item.Err)
  $text+=$out
  if($item.Process.HasExited){
   $item.Process.WaitForExit();$code=$item.Process.ExitCode
   Write-Activity ($item.Kind+' finished with exit code '+$code+'.')
   Write-Activity $out
   $item.Process.Dispose();$script:task=$null
   Refresh-Setup
  }
 }
 $ui.Log.Text=$text
 $ui.Log.ScrollToEnd()
})
$window.Add_Closing({Save-Settings;$timer.Stop()})
Write-Activity 'Dead Space launcher ready. Select your Xperia game ZIP and optional Reanimated APK.'
Write-Activity 'QEMU is retained for development; the packaged Windows version will not require WSL2 or Docker.'
Write-Activity 'Assets install without extra tools; standalone Windows game execution is still under development.'
Show-Page 'Home';Refresh-Setup;$timer.Start()
[void]$window.ShowDialog()
