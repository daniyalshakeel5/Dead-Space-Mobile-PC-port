# Player dependency decision — 2026-11-02

**Product requirement:** Installing and playing Dead Space Mobile on
Windows 10/11 must NOT require WSL2, Docker Desktop, Git, Python,
a terminal, or a user-managed Linux VM. Those are developer tools.

QEMU Linux user mode is supported on Linux/BSD hosts. The successful
EapRules QEMU game boot is Linux/x86-64 evidence, NOT a standalone
Windows executable. A Windows QEMU user-mode process cannot simply
use the Linux `qemu-arm` binary. Two compatible strategies remain:

1. **Linux portable game:** package qemu-arm, the 32-bit ARM loader
   and complete redistributable ARMHF SDL/GLES1/Mesa userland in a
   verified portable archive. Game source files stay on disk, local.
   No Docker required to RUN; CI may use containers to BUILD.
2. **Windows full-system QEMU:** package qemu-system-arm and a fully
   licensed ARM Linux runtime with interactive video, audio and
   DualShock 4 input. Must actually boot original Xperia game before
   enabling Play. This may be much slower than user-mode translation
   and must be benchmarked on low-end hardware.

If Windows full-system QEMU proves impractical, use the separate
Dynarmic native-host translator under development, but keep QEMU
as the Linux / testing backend. Never promise Windows user-mode QEMU
without a supported Windows Linux-syscall compatibility layer.

The WPF launcher still supports the exact Xperia game ZIP and optional
Reanimated HD APK. The old WSL2/Docker button is disabled and no
WSL2/Docker bootstrap is included in the distributable Windows ZIP.

Upstream sources:
- QEMU user-mode: https://www.qemu.org/docs/master/user/main.html
- QEMU ARM full-system: https://www.qemu.org/docs/master/system/target-arm.html
- GPL verified loader: https://github.com/EapRules/deadspace-native-arm
