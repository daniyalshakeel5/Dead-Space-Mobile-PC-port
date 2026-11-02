' Asset-free Dead Space WPF launcher bootstrap. Double-click to launch GUI silently.
Option Explicit
Dim shell, fs, base, cmd
Set shell = CreateObject("WScript.Shell")
Set fs = CreateObject("Scripting.FileSystemObject")
base = fs.GetParentFolderName(WScript.ScriptFullName)
cmd = "powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & base & "\Launcher.ps1"""
shell.Run cmd, 0, False
