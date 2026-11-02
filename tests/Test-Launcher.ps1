# Windows: powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File tests/Test-Launcher.ps1
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $repo 'launcher'
. (Join-Path $folder 'Core.ps1')
foreach($path in @('Core.ps1','Installer.ps1','Launcher.ps1')) {
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
foreach($control in @('GamePath','HdPath','BrowseZip','BrowseApk','InstallAssets','Launch','HomeNav','GraphicsNav','ControlsNav','DepsNav','Log','EnableHd','CopyLog')){
 if(-not $window.FindName($control)){throw ('Required adapted Diamond Rush launcher control absent: '+$control)}
}
if($window.FindName('Launch').IsEnabled){throw 'Play must remain DISABLED until original ARM runtime can boot'}
if($window.Title -notlike '*Dead Space*'){throw 'Wrong launcher identity'}
$window.Close()
Write-Output 'PASS: actual Diamond Rush WPF architecture, Dead Space controls/theme, no playable-engine claim, verified zip path safety, PowerShell parsing.'
