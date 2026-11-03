# Windows: powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File tests/Test-Launcher.ps1
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $repo 'launcher'
. (Join-Path $folder 'Core.ps1')
foreach($path in @('Core.ps1','Installer.ps1','Launcher.ps1','QemuBoot.ps1')) {
 $tokens=$null;$errors=$null
 [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $folder $path),[ref]$tokens,[ref]$errors)
 if($errors.Count){throw ('PowerShell parse failure '+$path+': '+($errors|Out-String))}
}
if((Quote-Argument 'C:\Program Files\Game Files\deadspace.zip') -ne '"C:\Program Files\Game Files\deadspace.zip"'){throw 'Command argument with spaces not quoted correctly'}
if((Quote-Argument 'C:\archive\') -ne '"C:\archive\\"'){throw 'Trailing path separator escaping failed'}
foreach($bad in @('../escape','dir/../escape','C:/Windows','x:ads','CON','AUX.txt','dir/end.','dir/end ','/absolute')) {
 $threw=$false
 try{Test-SafeArchivePath $bad}catch{$threw=$true}
 if(-not $threw){throw ('Unsafe archive path was accepted: '+$bad)}
}
foreach($good in @('assets/published/data/level_index.sb','lib/armeabi/libEAMGameDeadSpace.so','wall_01.ktx')){
 Test-SafeArchivePath $good
}
Add-Type -AssemblyName @('PresentationFramework','PresentationCore','WindowsBase')
[xml]$xaml=Get-Content -LiteralPath (Join-Path $folder 'Launcher.xaml') -Raw -Encoding UTF8
$window=[Windows.Markup.XamlReader]::Load((New-Object Xml.XmlNodeReader $xaml))
foreach($node in $xaml.SelectNodes('//*[@Name]')) {
 if(-not $window.FindName($node.Name)){throw ('Unbound WPF control: '+$node.Name)}
}
foreach($control in @('GamePath','HdPath','BrowseZip','BrowseApk','InstallAssets','QemuBoot','QemuStatus','OpenBootLogs','Launch','HomeNav','GraphicsNav','ControlsNav','DepsNav','Log','EnableHd','CopyLog')){
 if(-not $window.FindName($control)){throw ('Required adapted Diamond Rush launcher control absent: '+$control)}
}
if($window.FindName('Launch').IsEnabled){throw 'Play must remain DISABLED until original ARM runtime can boot'}
if($window.FindName('QemuBoot').IsEnabled){throw 'QEMU boot check must remain DISABLED until original Xperia game files are installed'}
# Standalone Windows package must never require WSL2 or Docker.
$guiText=Get-Content -LiteralPath (Join-Path $folder 'Launcher.xaml') -Raw
if($guiText -match 'Docker Desktop|WSL2 Linux|enable integration'){
 throw 'Windows launcher is still advertising external development dependencies'
}
$source=Get-Content -LiteralPath (Join-Path $folder 'Launcher.ps1') -Raw
if($source -match '\$ui\.QemuBoot\.IsEnabled=\s*-not \$busy'){
 throw 'QEMU developer-only boot test must not be enabled in player launcher'
}
if($source -notmatch [regex]::Escape('$ui.Launch.Add_Click')){
 throw 'Experimentally launchable original ARM executable is not wired to the Play button'
}
if($source -match [regex]::Escape('$ui.Launch.IsEnabled=$false')){
 throw 'Original game remains hard-disabled despite matching bundled runtime and installed assets'
}
if($window.Title -notlike '*Dead Space*'){throw 'Wrong launcher identity'}
$window.Close()
Write-Output 'PASS: actual Diamond Rush WPF architecture, Dead Space controls/theme, no WSL2 or Docker player requirements, no interactive-play claim, safe zip paths, PowerShell parsing.'
