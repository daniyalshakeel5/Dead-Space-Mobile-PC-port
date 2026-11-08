# Port source

This is the canonical source tree for the desktop port and its tooling.
Start with the [technical build guide](../docs/TECHNICAL_BUILD_GUIDE.md).

| Directory | Contents |
| --- | --- |
| `experimental/lifted_milestone6/tools/` | Current native ARM-to-C translator, ELF loader, compatibility runtime, SDL graphics/input/audio, Linux sensors, Windows support and diagnostics |
| `experimental/lifted_milestone6/src/` | Earlier C++/Unicorn host, retained for historical investigation; not the current native Linux executable |
| `experimental/windows_native_host/` | Windows substrate and build/install/repair tools |
| `packaging/` | Linux touchscreen and Windows controller distribution launchers |
| `launcher/` | WSL bootstrap launcher |
| `scripts/` | Build/run and gameplay evidence helpers |
| `tests/` | Asset-free translator/runtime/sensor/audio regression tests |
| `tools/` | Archive analysis, historical source import and trace comparison tools |

The tree contains the port implementation, generators and tests. Building the
actual game additionally requires the user's compatible original APK. The
original game library, assets and generated game-derived C are local inputs
and build products, not included here. `.github/workflows/` remains at the
repository root as required by GitHub Actions.

Old root-level source paths have moved under `source/`. Relative paths inside
the source tree and installed packages keep their previous layout. Use current
checkout paths in the technical guide; older milestone notes describe older
packages and are not the current Linux build procedure.
