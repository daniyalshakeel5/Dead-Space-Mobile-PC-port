# Private APK resource-format research

This project is a **native Windows/Linux rebuild in progress**. We have
working and tested native input, timing, ZIP validation and opt-in asset
installation tools, but **no recovered game loop or playable desktop game**.

The new `tools/deadspace_asset_recon.py` is a read-only research tool. It
reads up to 64 decompressed bytes from a bounded number of ZIP entries,
looking for known signatures: PNG, JPEG, DDS, PVR3, KTX, PKM, ASTC, Ogg,
WAV, FLAC, ZIP, ELF, DEX, and several other containers. It does **not**
decode those assets or assume that a guessed signature proves a game format.

## Run locally with your own APK (works on Windows and Linux)

This is the easiest way to use the research tool **without uploading any
private game files to public GitHub**. Requires Python 3.10+ only; uses
the standard library. The report is safe to review before sharing.

Windows PowerShell:

```powershell
py -3 tools/deadspace_asset_recon.py --apk "D:\\Games\\DeadSpaceMobile.apk" --output "deadspace-resource-recon.json"
```

Linux:

```bash
python3 tools/deadspace_asset_recon.py --apk "$HOME/Games/DeadSpaceMobile.apk" --output "deadspace-resource-recon.json"
```

**Only the JSON report is meant to be shared.** The original APK stays
where it is on your machine. Reports contain only top-level group counts,
extension group sizes, recognized magic categories, unidentified large
file counts, skipped-entry counts and the input APK's SHA-256 hash.
No original filenames, content prefixes, audio, textures, Java code, or
native libraries are serialized into the report.

The APK SHA-256 is a strong identifier of the particular package build;
remove it before sharing if you do not want that identifier published.
No decoder or game behavior is verified by these aggregate categories.

## GitHub-hosted private analysis

The existing `.github/workflows/deadspace-research.yml` manual run will,
once `DS_APK_URL` is configured, run **both**:
- `deadspace_audit.py`: native ABI, imported Android APIs, library metadata
- `deadspace_asset_recon.py`: resource-group and file-magic statistics

The optional `DS_APK_URL` secret must be a private short-lived HTTPS link
to an authorized copy of the user's APK. The runner uses temporary
storage and uploads only `report/*.json`. Do not commit game binaries
or grant public URLs. The workflow needs to be available on the default
branch before GitHub supports manual dispatch from Actions.

Push/PR actions run synthetic fixture tests and a **repository-only**
audit without original content. A green CI check does not mean the APK
has been analyzed.

## Native-library entrypoint and loader API evidence

The audited ELF import evidence is now more targeted than a library count.
On **your machine** or a properly configured **private** runner, run:

```sh
python3 tools/deadspace_audit.py --root . --apk "/path/to/owned.apk" --output "native-evidence.json"
```

For each native library, this records the actual ELF class, endianness, machine
architecture, shared-library type (`ET_DYN` or otherwise), ELF entry address,
linked library names and any available JNI/bootstrap symbols. The
`platform_api_import_evidence` field lists only known public platform
imports in these families: Android `AAssetManager` resource access, standard
file I/O, decompression, GLES texture upload, GLES draw calls, EGL contexts
and audio. Broad heuristic `import_hint_counts` are **counts**, not private
symbol names. Native ELF data is inspected in a temporary folder, not copied
into a report.

**Evidence rules:** JNI exports such as `JNI_OnLoad` identify potential
bootstrap points; an ELF `e_entry` is **not** the frame update loop. An
`AAssetManager_open` import proves a dependency on that API, **not** which
resource files it reads. A GLES texture call is **not** a decoded proprietary
texture format. Stripping, static linkage and runtime JNI registration can
hide original function boundaries. This report must be cross-checked against
actual private disassembly and resource samples before implementing loaders.

The native report includes library paths, JNI export names, and build hashes,
so **review it before sharing**; unlike the aggregate resource-recon report,
it is not designed to be fully name-anonymized. Never upload or commit native
library binaries, APKs, full symbol dumps or disassembled original code.

## What findings mean for implementation

- Numerous DDS/PVR/KTX/PKM/ASTC entries suggest relevant texture decoders;
  **choose none** until confirmed by the actual file headers and loader code.
- Ogg/WAV/FLAC entries suggest candidate audio formats; not an audio engine.
- Large unrecognized entries may be game-specific packed containers;
  before writing parsers, inspect their formats privately and identify
  matching routines in the original `libDeadSpace.so`.
- Android/ELF/JNI counts help prioritize native engine boundary research.

## Next verification gate

Analyze the actual authorized APK and cross-check suspected formats against
the game library's real asset readers. Only then implement a specific
decoder with private and synthetic regression cases. Keep gameplay
attributions and performance claims unverified until original observation.

The user expressly excluded `dspc_milestone4.zip`; no data from that
archive was used.
