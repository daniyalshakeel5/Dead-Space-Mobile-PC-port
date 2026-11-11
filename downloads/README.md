# Windows Desktop Repair R2 Candidate

[Download the R2 Windows ZIP](DeadSpaceMobile-Windows-Desktop-Fix.zip?raw=true)

**The first desktop candidate is withdrawn:** the owner reported cursor trails
after clicking outside and inability to resize in windowed mode.

R2 removes the framebuffer cursor overlay and custom borderless-window code.
It selects the native Windows arrow only inside the game client and restores
SDL-managed desktop fullscreen and native window resizing. F11 restores the
saved window placement. Resize-border cursors remain Windows-managed.

The GUI executable, hidden launcher, matched input release before pause,
unfocused input suppression and pending resize on resume are retained.
Native controls, grab gestures, audio, sensor and threading changes match the
campaign-tested gesture package; no gameplay changes were made for R2.

Copy the update files into the existing installation, keeping `game` and
`userdata`. Back up `userdata` before testing. No APK, game assets, generated
game C or saves are bundled.

## Verification

**R2 is a candidate, not yet Windows-runtime certified.** The approved Windows
workflow is triggered by this upload; see the branch's Actions run for status.

All 32 local regressions pass. The new native-cursor fixture checks client-only
handling, delegation of native resize cursors, capture/focus refresh and no
per-frame cursor resets. The complete native Windows GUI game and synthetic
Windows hosts link successfully. GUI subsystem 2 and package checksums pass.

The Windows integration checks now require the native arrow, thick resize-frame
style and bottom-right resize hit testing before and after F11, real click-away
cycles, background resize and desktop capture with separate native-cursor
composition. No framebuffer cursor pixels are expected or drawn by the game.

Actual OBS hooks/encoders, ShareX configurations and multi-monitor hardware
still need verification. Use the game process/window, not PowerShell, and
enable the recorder's Capture Cursor option. OpenGL generally needs Game
Capture or modern Windows Graphics Capture; legacy BitBlt-only capture may fail.

ZIP SHA256: `980f24224fc3343f5e86f8949802dec360b97013f63042d621fe143e74b334eb`

EXE SHA256: `6714effe68b455702538bf70898e86ecdfd64b82a557d86056cf23358155f94c`

## Known-Good Fallback

[Campaign-tested gesture build](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/raw/0e44209bf87ff0f5f4bfc479730ce3f47ee14c6e/downloads/DeadSpaceMobile-Windows-Gesture-Fix.zip)
is retained unchanged. It predates the desktop repairs; the owner completed the
campaign on that build, not on R2.
