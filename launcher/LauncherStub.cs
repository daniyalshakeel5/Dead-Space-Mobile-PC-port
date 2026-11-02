using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
public static class DeadSpaceLauncherEntry {
  [STAThread]
  public static void Main(string[] args) {
    try {
      string root=Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
      string script=Path.Combine(root,"launcher","Launcher.ps1");
      if(!File.Exists(script)) throw new FileNotFoundException("Launcher.ps1 was not packaged beside DeadSpaceLauncher.exe",script);
      var start=new ProcessStartInfo();
      start.FileName=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32","WindowsPowerShell","v1.0","powershell.exe");
      start.Arguments="-NoProfile -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File \""+script.Replace("\"","\\\"")+"\"";
      start.WorkingDirectory=root;
      start.UseShellExecute=false;
      start.CreateNoWindow=true;
      start.WindowStyle=ProcessWindowStyle.Hidden;
      Process.Start(start);
    } catch(Exception error) {
      MessageBox.Show(error.Message,"Dead Space Launcher",MessageBoxButtons.OK,MessageBoxIcon.Error);
    }
  }
}
