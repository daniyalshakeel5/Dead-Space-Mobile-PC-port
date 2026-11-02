param(
 [Parameter(Mandatory=$true)][string]$GameDir,
 [Parameter(Mandatory=$true)][string]$BackendScript,
 [Parameter(Mandatory=$true)][string]$LogsDir
)
# Invoked by DeadSpaceLauncher's WPF UI in a hidden Windows PowerShell.
# It is an OPT-IN headless startup verification, not interactive Play.
# GPL-compatible source is obtained/build by the Python adapter inside WSL.
$ErrorActionPreference='Stop'

function Fail-Boot([string]$Message) {
 Write-Output ('QEMU_BOOT_BLOCKED: '+$Message)
 exit 2
}
function Wsl-Path([string]$WinPath) {
 $canonical=[IO.Path]::GetFullPath($WinPath)
 $lines=@(& $script:WslExe --exec wslpath -a -u $canonical 2>&1)
 if($LASTEXITCODE -ne 0 -or $lines.Count -eq 0) {
  throw ('WSL could not access this Windows folder: '+$canonical+'. Check Windows drive mounting in WSL.')
 }
 $value=[string]($lines | Select-Object -Last 1)
 $value=$value.Trim()
 if(-not $value.StartsWith('/')) {
  throw ('WSL returned a non-Linux filesystem path: '+$value)
 }
 return $value
}
try {
 if(-not (Test-Path -LiteralPath $GameDir -PathType Container)) {
  throw 'Xperia Play game files have not been installed.'
 }
 if(-not (Test-Path -LiteralPath $BackendScript -PathType Leaf)) {
  throw 'The QEMU backend script was not packaged with this launcher.'
 }
 # Never display a stale successful game-boot report after a newer
 # attempt fails before it reaches Python (e.g., missing Docker).
 [IO.Directory]::CreateDirectory($LogsDir) | Out-Null
 $priorReport=Join-Path $LogsDir 'boot-status.json'
 if(Test-Path -LiteralPath $priorReport -PathType Leaf) {
  Remove-Item -LiteralPath $priorReport -Force
 }
 $script:WslExe=Join-Path $env:SystemRoot 'System32\wsl.exe'
 if(-not (Test-Path -LiteralPath $script:WslExe -PathType Leaf)) {
  throw 'Windows Subsystem for Linux (WSL2) is not installed.'
 }
 # These probes run only in a background process, never block the WPF UI.
 Write-Output 'Checking Windows Subsystem for Linux (WSL2)...'
 $pythonCheck=@(& $script:WslExe --exec python3 --version 2>&1)
 if($LASTEXITCODE -ne 0) {
  throw 'Python 3 is not available inside the default WSL distribution.'
 }
 Write-Output ('Detected '+(($pythonCheck -join ' ').Trim()))
 $dockerCheck=@(& $script:WslExe --exec docker info '--format' '{{.ServerVersion}}' 2>&1)
 if($LASTEXITCODE -ne 0) {
  throw 'Docker Linux containers are not available to WSL. Start Docker Desktop and enable integration with your Linux distribution.'
 }
 Write-Output ('Docker Linux backend ready: '+(($dockerCheck -join ' ').Trim()))
 [IO.Directory]::CreateDirectory($LogsDir) | Out-Null
 $linuxGame=Wsl-Path $GameDir
 $linuxBackend=Wsl-Path $BackendScript
 $linuxLogs=Wsl-Path $LogsDir
 Write-Output 'Xperia original ELF: private game data mounted read-only.'
 Write-Output 'Upstream runtime: pinned GPL-3.0 EapRules ARM/Linux loader.'
 Write-Output 'Translator: qemu-arm, 32-bit ARM Linux user mode.'
 Write-Output 'Verification: original JNI_OnLoad, NativeOnCreate, GLES and content.'
 Write-Output 'Building and running may require Docker to download dependencies.'
 $arguments=@('--exec','python3',$linuxBackend,
   '--game-dir',$linuxGame,'--logs',$linuxLogs,'--allow-external-build')
 # No arbitrary shell command or eval: each user-selected path stays one
 # argument, even when the Windows directory contains whitespace.
 & $script:WslExe @arguments 2>&1 | ForEach-Object {Write-Output ([string]$_)}
 if($LASTEXITCODE -ne 0) {
  throw ('The QEMU backend exited with code '+$LASTEXITCODE+'. Inspect the QEMU diagnostics in '+$LogsDir)
 }
 Write-Output 'QEMU original-game boot verification completed successfully.'
 exit 0
}catch {
 Write-Output ('QEMU_BOOT_BLOCKED: '+$_.Exception.Message)
 exit 2
}
