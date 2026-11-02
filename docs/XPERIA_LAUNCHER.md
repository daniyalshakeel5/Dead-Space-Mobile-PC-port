# Dead Space Mobile — Windows launcher

Native Windows 10/11 WPF setup launcher **adapted directly from our Diamond Rush N73 v0.7.0 launcher** (`DiamondRush-N73-Launcher-v0.7.0.zip`, user-owned). Carries across the sidebar architecture, card-based install workflow, local persistent settings, process-tracked installation, activity log, quote-safe argument passing, and dependency status. Restyled with Dead Space's charcoal / distressed red / holographic cyan palette. The art is programmatic WPF; no EA/Ginestro images, textures, or logos are bundled.

## First use

Double-click **`DeadSpaceLauncher.exe`**. No Python, Java, or command-line work is required on standard 64-bit Windows 10/11 with Windows PowerShell 5.1 and WPF.

In the Home / Setup screen, select your private, original **Xperia Play Vita-ready `deadspace.zip`** (required) and **Dead Space Reanimated v1.2 or v1.3 `.apk`** (optional). Click **Install / Verify Game Files**. If neither file is selected, the launcher walks you through choosing them using file dialogs. Its installation process runs in the background with live diagnostics and copies only validated assets to `%LOCALAPPDATA%\DeadSpaceMobile-Xperia\installed`.

The Xperia 1.1.33 ARM game and ATC campaign assets remain under `assets/` and `lib/armeabi/`; Reanimated's 222 HD `.ktx` textures remain **separate** under `enhancements/reanimated/texture_hd/`. SHA256/version, key original resource hashes, ZIP path safety, and size bounds are checked before any installation; the new folder is populated transactionally. This matches the functionality of the repo's Python importer but **does not require Python on the user's PC**.

The Graphics screen remembers a future preference for HD textures; the Controls screen documents DualShock 4 touchpad, sticks and buttons. The Runtime page shows the selected QEMU compatibility backend and reports the last real startup milestone.

## End-user runtime rule: no WSL2, Docker, Git or Python

**The public Windows launcher must not ask people to install developer infrastructure.**
It verifies and installs the private Xperia Play `deadspace.zip` and
optional Reanimated HD APK with Windows PowerShell/WPF only.

The original Linux `qemu-arm` user-mode translator cannot simply run
as a Windows process. Running that backend inside WSL2/Docker is useful
for developers and for upstream verification but is not a shipping
Windows game. The old opt-in WSL2/Docker boot check is therefore
**removed from the player-facing package**; its engineering files
remain available in the source tree.

The Windows full-system QEMU alternative would require a bundled ARM
Linux guest, the verified GPL game loader, a compatible userland and
graphics/audio/controller output. Packaging this is not yet complete,
and software emulation of ARM graphics may perform poorly on low-end
PCs. It is **not** acceptable to ship a fake Play button or require
users to assemble a VM themselves.

**Play remains disabled** until a self-contained runtime demonstrably
boots the actual game and supports an interactive window. Asset
installation remains fully functional. The Linux backend can instead
be distributed later as a portable `qemu-arm` bundle without Docker.

The older QEMU boot script `tools/xperia_qemu_boot.py` is retained
strictly for engineering verification, not included in the Windows
user ZIP. Likewise `launcher/QemuBoot.ps1` is not shipped. The
official release must not embed the user's copyrighted original files.

The installed files and logs are local. Never upload or commit your original ZIP, APK, textures, game binary or game sound files to this repository. Reanimated imagery and original assets belong to their owners. Diamond Rush launcher architecture is based on the user's original project.

## Developers

Source: `launcher/Launcher.xaml`, `launcher/Launcher.ps1`, `launcher/Core.ps1`, `launcher/Installer.ps1`, `launcher/LauncherStub.cs`.

The Windows CI workflow compiles a minimal native `.exe` bootstrapper that opens the bundled WPF launcher without flashing a command line. It also validates the actual WPF XAML controls and PowerShell syntax. The backend's original archive is identified by SHA256 `04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31`. The Reanimated official v1.2/v1.3 SHA256s are pinned. No original contents are ever embedded in the installer or Windows ZIP.

Licensing: no third-party runtime source or copyrighted game assets are included in this WPF package. Credits to the Diamond Rush desktop launcher work, the original Dead Space Mobile developers, and Reanimated creator GINESTR0.
