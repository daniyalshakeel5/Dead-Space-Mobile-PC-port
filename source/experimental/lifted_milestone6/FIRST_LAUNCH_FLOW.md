# Dead Space Mobile — first-run versus subsequent-launch boot flow

## Expected behavior (reported from the original game)

A fresh installation **does not begin at the main menu**:

1. Show the difficulty selection screen.
2. After choosing difficulty, play the opening cutscene.
3. Transition from the cutscene into first gameplay.
4. Save first-run progress in the application's writable data directory.
5. On **subsequent launches**, the main menu becomes the expected entry point.

This sequence was supplied by the original-game tester. It is not yet proven to work end-to-end in the native PC build.

## Native port requirements

- Keep `DS_ASSETS` (original extracted APK resources) separate from `DS_DATA` (writable persistent game data).
- Never delete `DS_DATA` on normal relaunch; do not regenerate a fresh-install state for each test.
- A first-launch boot should be assessed by difficulty selection, intro cutscene, and gameplay, **not** by main-menu visibility.
- Main-menu functionality should be checked only in a second process launch with the **same** `DS_DATA` after the game has actually persisted first-run progress.
- A nonblank 120-frame SDL capture demonstrates native rendering only. It does **not** prove difficulty selection, the cutscene, gameplay, or the main menu.
- Existing scripted test taps are exploratory input, not verified selections of a difficulty or completion of the intro.

## CI checks

The Linux real-APK runner creates `$RUNNER_TEMP/dspc-live/userdata/var` independently of the extracted game assets and passes it through `DS_DATA`. It captures a first-run frame. If real progress files exist after a successful first run, it performs a second launch with the **same** data directory and captures a separate frame. A second-launch smoke pass still requires visual inspection before claiming the main menu has loaded.

The Windows fresh installer uses `<install>/userdata` for `DS_DATA` and does not overwrite it when the game starts again.
