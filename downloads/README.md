# Windows Test Build

[Download DeadSpaceMobile-Windows.zip](DeadSpaceMobile-Windows.zip?raw=true)

Native Windows x86 test build compiled from release tag `Linux-with-controller-support`, source commit `b1fa1ca87922cd905da1be2521e412e0758b4d7e`, with the source changes included in the ZIP's `windows-build-changes.patch`.

Compilation, launcher syntax, and original-APK extraction checks passed. Gameplay remains unverified. The latest Linux RAR omits its native controller implementation's source, so this build uses the older committed touchscreen/keyboard controller frontend. It is not a full reproduction of the latest Linux controller release.

Extract the ZIP, run `Play Dead Space.cmd`, and select the matching upstream Fixes APK. No original APK, assets, saves, or generated game C are included.

ZIP SHA256: `f3a388aeb485ab0fbca6b55c662da552c3d5ac23f556561d8338334963c70ad5`
