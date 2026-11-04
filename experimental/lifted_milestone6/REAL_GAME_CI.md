# Real-game-native CI only

The public source repository does not contain copyrighted original EA assets.

To run authentic end-to-end tests, store your own Deas.Space.Mobile.Fixes.apk (325,769,461 bytes) and deadspace.zip (145,903,794 bytes) as GitHub Release assets in a separate PRIVATE repository, for example daniyalshakeel5/Dead-Space-Mobile-Assets. Git files have a 100 MB individual size limit; release assets can be larger.

Set up a private repository release tagged native-game-inputs, attach both exact files. In the public source repository add an Actions secret DS_PRIVATE_ASSETS_TOKEN. It must be a fine-grained GitHub token with Contents: Read permission on that private assets repository. Never expose it in a source file or workflow input.

The real-game workflow uses the token to download the original files only into an ephemeral GitHub runner. It builds the actual native i686 game and launches the game in SDL/OpenGL under Xvfb with real touch inputs, running 900 frames and checking a captured framebuffer. Missing game inputs, compilation failure, a crash, or blank output return failure. The workflow does not upload APK, game data or screenshots as artifacts.

A successful smoke run proves only that a real game executable ran with its assets for 900 frames. It is NOT proof of sound, level loading, full game interaction or the complete gameplay loop.

The currently connected GitHub tool can edit the source repository but cannot create a new private repo or upload Release assets. Those two setup steps must be performed through GitHub or GitHub CLI with an account that has access to the game files.
