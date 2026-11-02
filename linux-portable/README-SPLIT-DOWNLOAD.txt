Dead Space Mobile - Linux QEMU test - smaller reliable downloads

The 289 MB GitHub Actions artifact was interrupted during download.
The same original-free Linux bundle is now published on the normal public
GitHub RELEASE page as individually retriable 24 MiB files.

1. Download 00-START-HERE.zip plus EVERY file ending .part.000, .part.001,
   etc. Save all of them in the SAME folder. Do not rename the files.
2. Extract 00-START-HERE.zip directly into that folder.
3. Right-click Assemble-DeadSpace-Linux.sh and select Run as Program.
   Or mark Assemble-DeadSpace-Linux.desktop trusted and double-click it.
4. The helper checks every part, identifies failures, reconstructs the
   ZIP, confirms the checksum, and extracts the Linux runtime.
5. Copy your own Xperia deadspace.zip next to the extracted
   DeadSpaceMobile-Xperia-Linux/Run Dead Space.sh, then run that script.

If one part fails, download only that part again.
No Docker, WSL2, Python, Git, compilers, sudo, or command-line
setup is needed on your device. The package excludes copyrighted
original EA game assets and optional Reanimated HD files.

This is an experimental x86-64 Linux test, not yet proven
to display interactive gameplay on your Linux device.
