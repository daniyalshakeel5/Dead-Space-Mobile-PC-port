# Dead Space Mobile — Dark WPF Launcher (experimental)

**Windows 10/11:** Double-click **`Start-DeadSpace-Launcher.vbs`** in the `launcher` folder. No command line, Java or Python required. This is an adaptation of the previously built Diamond Rush N73 v0.7.0 launcher architecture, not a replacement.

Under **Home / Setup**, choose the exact Xperia Play `deadspace.zip` and your Reanimated v1.2/v1.3 APK if desired, then click **Install / Verify Game Files**. The launcher checks archive versions and extract safety, then writes original files to `%LOCALAPPDATA%\DeadSpaceMobile-Xperia\installed`, keeping Reanimated HD files in a separate enhancement folder. Do not share the user-owned APK or ZIP publicly.

The **Play** button is intentionally disabled: the original game's ARM32 JNI/GLES runtime isn't playable on x86-64 yet. The installer is operational; this bundle does not purport to include a game binary. It can be packaged with that runtime later without changing the UI.

This source-only portable ZIP was created from the user's Diamond Rush launcher v0.7.0 with a restyled WPF UI and original ZIP + Reanimated HD input. Windows CI and compilation for an `DeadSpaceLauncher.exe` entry stub are configured in `.github/workflows/xperia-windows-launcher.yml` but the checked-in source still needs Windows execution confirmation if no CI build is linked.
