# Dead Space Mobile: authentic Linux i686 game test

The original game APK is now in the user's existing GitHub release:

https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/releases/tag/apk-files-for-testing

Three game variants are available:
- **Deas.Space.Mobile.Fixes.apk** — original host's expected `libDeadSpace.so`, first integration target.
- **deadspace.xperia.APK.apk** — separate Xperia Play game binary, not interchangeable with the original host.
- **DeadSpaceReanimated-1.3.apk** — another modified game variant, not silently mixed with the first target.

The real-game workflow at .github/workflows/deadspace-real-game-runtime.yml is **on the default branch** and runs without any private token. It downloads the first APK from the exact existing release on a temporary GitHub runner and builds the translated native i686 game. It launches SDL/OpenGL under a virtual display using genuine APK assets, sends touch inputs, checks the native frame loop and verifies a non-flat framebuffer. Missing assets, translation/compiler errors, runtime crashes, timeouts, or flat output are FAILURES. No synthetic fixture is acceptable for runtime claims.

Invoke manually in GitHub Actions, or update the workflow on main to invoke the push trigger. The initial genuine run is:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41853284230

Unlike the old asset-free synthetic workflows, this requires no separate upload to a private repo, but note **the release is currently public** and contains copyrighted game binaries uploaded by the repository owner. Do not copy or attach game binaries to newly published source commits or public CI artifacts. Separate Xperia/Reanimated engine compatibility checks must examine the actual ELF library and manifest before attempting a rebuild.

Success establishes only a native boot/frame smoke test, not complete gameplay, sound, AI, interaction, saves or level progression.
