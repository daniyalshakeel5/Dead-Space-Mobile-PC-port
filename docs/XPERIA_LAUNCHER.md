# Dead Space Mobile — Windows launcher

Native Windows 10/11 WPF setup launcher **adapted directly from our Diamond Rush N73 v0.7.0 launcher** (`DiamondRush-N73-Launcher-v0.7.0.zip`, user-owned). Carries across the sidebar architecture, card-based install workflow, local persistent settings, process-tracked installation, activity log, quote-safe argument passing, and dependency status. Restyled with Dead Space's charcoal / distressed red / holographic cyan palette. The art is programmatic WPF; no EA/Ginestro images, textures, or logos are bundled.

## First use

Double-click **`DeadSpaceLauncher.exe`**. No Python, Java, or command-line work is required on standard 64-bit Windows 10/11 with Windows PowerShell 5.1 and WPF.

In the Home / Setup screen, select your private, original **Xperia Play Vita-ready `deadspace.zip`** (required) and **Dead Space Reanimated v1.2 or v1.3 `.apk`** (optional). Click **Install / Verify Game Files**. If neither file is selected, the launcher walks you through choosing them using file dialogs. Its installation process runs in the background with live diagnostics and copies only validated assets to `%LOCALAPPDATA%\DeadSpaceMobile-Xperia\installed`.

The Xperia 1.1.33 ARM game and ATC campaign assets remain under `assets/` and `lib/armeabi/`; Reanimated's 222 HD `.ktx` textures remain **separate** under `enhancements/reanimated/texture_hd/`. SHA256/version, key original resource hashes, ZIP path safety, and size bounds are checked before any installation; the new folder is populated transactionally. This matches the functionality of the repo's Python importer but **does not require Python on the user's PC**.

The Graphics screen remembers a future preference for HD textures; the Controls screen documents DualShock 4 touchpad, sticks and buttons. The Runtime page shows the selected QEMU compatibility backend and reports the last real startup milestone.

**QEMU is the primary original-game boot backend.** Once the Xperia
game is installed, the Home page enables **TEST GAME BOOT (QEMU)**.
The button first asks permission to download/build the separate
EapRules **GPL-3.0** ARM/Linux runtime. A background Windows PowerShell
bridge checks **WSL2 and Docker Linux containers**, converts the
user's installed game folder into a WSL filesystem path, and invokes
the bundled Python boot adapter. **Python 3, Git and Docker are needed
inside WSL2**, but none is required merely to open the Windows WPF
launcher or install assets. All diagnostics are stored under
`%LOCALAPPDATA%\\DeadSpaceMobile-Xperia\\qemu-boot`.
The Runtime page provides a button to open these logs. The UI
distinguishes success at real `JNI_OnLoad` / `NativeOnCreate` from
failure before the engine booted; stage M7 verifies input-driven
scene changes.

**Play is still disabled**, because this first QEMU compatibility
adapter renders **offscreen** into its test environment and doesn't
yet present an interactive Windows game window or mapped DualShock 4.
Do not confuse passing the game-boot test with a playable release.
The first interactive display/audio/controller path is still
outstanding. The Reanimated HD KTX sidecar is prepared but not
yet mounted into the original game's renderer.

The WPF launcher remains usable with no external dependencies until
the user explicitly chooses the QEMU test. Its boot adapter is bundled
with the portable Windows package at `tools/xperia_qemu_boot.py`,
and the WSL2 bridge is in `launcher/QemuBoot.ps1`.
No proprietary original game data enters the Docker *build image*,
GitHub source, releases or Actions artifacts; installed content is
mounted read-only into the emulator container.

The installed files and logs are local. Never upload or commit your original ZIP, APK, textures, game binary or game sound files to this repository. Reanimated imagery and original assets belong to their owners. Diamond Rush launcher architecture is based on the user's original project.

## Developers

Source: `launcher/Launcher.xaml`, `launcher/Launcher.ps1`, `launcher/Core.ps1`, `launcher/Installer.ps1`, `launcher/LauncherStub.cs`.

The Windows CI workflow compiles a minimal native `.exe` bootstrapper that opens the bundled WPF launcher without flashing a command line. It also validates the actual WPF XAML controls and PowerShell syntax. The backend's original archive is identified by SHA256 `04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31`. The Reanimated official v1.2/v1.3 SHA256s are pinned. No original contents are ever embedded in the installer or Windows ZIP.

Licensing: no third-party runtime source or copyrighted game assets are included in this WPF package. Credits to the Diamond Rush desktop launcher work, the original Dead Space Mobile developers, and Reanimated creator GINESTR0.
