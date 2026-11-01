# Local APK asset installation (native Windows/Linux)

**What this does:** Build and use a native C++ utility to extract only
`assets/` from **your own legitimately obtained** Dead Space Mobile APK to a
new private directory. Game content is **never** committed or included in
public releases, PRs, or GitHub Actions artifacts. It is not yet a game engine.

## Install on your own PC

The GitHub Actions workflow
`.github/workflows/asset-installer.yml` compiles `ds_asset_install`
and `ds_apk_probe` for Windows and Linux and tests against synthetic
fixture archives. Download the appropriate OS artifact **only after
the new workflow has passed**. These executables contain no game assets.

On Windows, from PowerShell:

```powershell
.\ds_asset_install.exe "D:\Games\DeadSpaceMobile.apk" "D:\Games\DeadSpaceMobileAssets"
```

On Linux, from a terminal:

```bash
chmod +x ./ds_asset_install
./ds_asset_install "/path/to/DeadSpaceMobile.apk" "$HOME/Games/DeadSpaceMobileAssets"
```

The chosen output directory must **not** exist yet. The installer makes it,
verifies each stored or raw-DEFLATE entry's CRC32 and expected size, and
removes the entire newly created output directory if extraction fails.
It only writes APK entries under `assets/`; it does not extract `classes.dex`,
`lib/`, `res/` or the Android manifest.

Run without arguments for a simple text prompt for APK and destination.

## Security and compatibility limits

- Read-only ZIP32 preflight rejects archive path traversal, unsafe
  separators/colon names, duplicate or case-colliding names, symlinks,
  encrypted/multidisk archives and ZIP64. This may reject legitimate unusual
  APKs; failure is not evidence the source game is broken.
- Data extraction supports ZIP compression method 0 (stored) or 8 (raw
  DEFLATE), via the platform's zlib. Original resources are **not decoded**.
- Single assets over 1 GiB or total assets over 8 GiB are refused.
- Extracted content's CRC32 is verified; this catches ordinary data damage
  but is **not cryptographic authentication**. APK signing certificates
  are not checked, and the tool does not verify provenance.
- Destination must be a new directory. Do not install into system folders,
  public Git repositories, shared cloud folders, or locations with concurrent
  untrusted writers. This is not a hardened sandbox against concurrent
  filesystem attacks.
- This install stage cannot infer how the game parses formats, performs
  animation, rendering, collision, AI or audio. Nothing is connected to
  a game renderer yet.

## GitHub runner boundaries

The CI uses synthetic, redistributable ZIP fixtures only and distributes
**installer source/binaries, never original game content**. It must not
download the APK or extract actual assets in public CI.

Next engineering step: use the privately installed user's resource files as
inputs to *evidence-driven* format readers; first identify native archive
readers and data layouts from the original game, then verify with tests.
Never invent game format contracts.

As requested, `dspc_milestone4.zip` remains out of scope and was not used.
