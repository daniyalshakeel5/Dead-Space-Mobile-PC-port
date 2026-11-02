# Xperia Play Vita-ready donor, confirmed November 2 2026

The user's private deadspace.zip is the exact 145903794-byte Vita donor.
SHA1: 61d51d8ba5374f97b5a4971a2d9d7da31baf840c
SHA256: 04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31

The archive contains the exact 4499468-byte Xperia 1.1.33 ARM ELF
(SHA1 0ed42b611415015807f759ec9b5457857143ce39).
There are 1745 game asset files (254747912 uncompressed bytes), 22 scene
map files, the audio set, game models and frontend textures.
The source ZIP contains its root under deadspace/.

IMPORTANT: EapRules' donor manifest identifies this exact build as
vita-rip-atc, not the full PVRTC build. ATC texture handling is needed.
The Vita-ready donor omits optional content but retains the main campaign.
Reference: https://github.com/EapRules/deadspace-native-arm

Local install, without uploading or distributing copyrighted assets:

    python -m tools.xperia_vita_donor_install --zip deadspace.zip --verify-only
    python -m tools.xperia_vita_donor_install --zip deadspace.zip --destination installed-xperia

The installer validates the entire ZIP SHA256 and key original resource hashes,
rejects traversal, ZIP symlinks, case collisions and suspicious sizes,
stages extracted assets transactionally, and refuses to overwrite installs.
Output: installed-xperia/assets and installed-xperia/lib/armeabi/libEAMGameDeadSpace.so.
An installer is not a gameplay runtime. ARM JNI imports and GLES/ATC support
are still required for a playable x86-64 build.

## Reanimated HD enhancement archive selection

The combined installer now **prompts for the Reanimated APK** as well as the
required Xperia ZIP. On a desktop, use Python's native file-selection dialogs:

```sh
python -m tools.xperia_vita_donor_install --gui
```

Alternatively, provide both paths explicitly (suitable for PowerShell,
Linux shells or a future compiled launcher):

```sh
python -m tools.xperia_vita_donor_install --zip deadspace.zip --reanimated-apk DeadSpaceReanimated-1.3.apk --destination installed-xperia
```

On a terminal, running with `--zip` and `--destination` but without
`--reanimated-apk` asks the user for the optional APK path. Enter skips
the HD enhancement, or pass `--no-hd` to explicitly install original-only.

The importer accepts only the verified official Reanimated v1.2/v1.3
releases (exact SHA256). The **real Reanimated v1.3 APK** passed an
isolated metadata-only GitHub Actions inspection:
https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41470766140.
The actual HD resource layer consists of **222 `.ktx` files** totaling
**340,222,792 uncompressed bytes**. The install tool extracts **only**
`assets/texture_hd/` into
`installed-xperia/enhancements/reanimated/texture_hd/`.
It never replaces Xperia Play's `assets/`, ARM library, audio or maps
with a game build from another revision. The combined installation
verifies both archives first and stages both layers as one atomic folder.

**Important:** this is resource preparation, **not working HD graphics
inside the Xperia game yet**. The source Reanimated APK contains a
different ARM revision, and its texture names/shaders aren't yet mapped
to Xperia's ATC material and M3G resources. The metadata explicitly
marks `enabled_in_runtime=false` until that mapping is independently
verified. Both source archives remain owned by the user and are not
committed to GitHub.


### Windows setup shortcut

If you cloned this experimental branch, double-click
`tools/Run-Xperia-Asset-Setup.cmd` to open both file selectors
(uses Python 3 and its built-in tkinter file dialogs).
Choose the original Xperia Play `deadspace.zip` first, and then the
Reanimated APK. The extraction destination defaults to a new
`DeadSpaceMobile-Xperia` folder in your chosen parent directory.
No console commands are required.

The 222 Reanimated files are **KTX containers**, not the ATC data directly
stored in the Vita donor. The HD renderer will additionally need to
identify KTX internal GPU formats, connect KTX names to game material
references, and upload/transcode those assets safely. Installation
does not make HD rendering functional yet.
