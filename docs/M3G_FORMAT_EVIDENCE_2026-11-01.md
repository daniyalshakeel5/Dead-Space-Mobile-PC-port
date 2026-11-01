# Dead Space Mobile M3G resource research — November 1, 2026

## Source and provenance

The developer branch uses the already-published upstream APK release
`RAFALAMAO/Dead-Space-Mobile-Fixes`, release asset ID `158932873`.
The user confirmed that this is the APK supplied privately in ChatGPT Library.
No game binaries, models, audio, source data, proprietary assets or native
libraries are stored in this port repository or any CI artifact. The APK is
downloaded into an ephemeral GitHub Actions workspace and removed on exit.

### Credited external research and format references

- [JSR-184 M3G file format (section framing, Adler32, header objects)](https://wupp.cc/j2meapi/JSR_184_Mobile_3D_Graphics_API_1_1/file-format.html)
- [Mobile Dead Space M3G model/texture discussion, ResHax (2025)](https://reshax.com/topic/1460-mobile-dead-space-m3g-modeltexture-bundles-help/): early observation that model resources use an IronMonkey-specific M3G variant, while other resources use the standard JSR-184 identifier.
- [RaduMC/M3G2FBX](https://github.com/RaduMC/M3G2FBX): conversion research for standard and related IM2/IM3 formats. Its approach is a reference, **not** a verified decoder for this game's exact IM-M3G variant. Preserve its original authors' credits and license if any code is subsequently adapted.
- [Original fixes author RAFALAMAO](https://github.com/RAFALAMAO/Dead-Space-Mobile-Fixes): the upstream APK fixes, separate from this independently developed desktop research.

## Findings: confirmed against real APK

Source: GitHub Actions push runs `41285804108`, `41286597401`,
`41286095902`, `41286351778` and subsequent research runs.

- One ARM32 ELF shared library (ELF32 ET_DYN); this is not x86-64 code.
- 265 files with extension `.m3g`, totaling 195,942,656 declared bytes.
- **174** use the exact JSR-184 12-byte identifier (125,907,703 bytes).
- **91** use `IM-M3G` identifier (70,034,953 bytes); neither IM2 nor IM3.
- The 91 IM-M3G first sections are **not** directly interpretable using
  standard JSR-184 framing. A bounded 1 MiB first-section probe saw 56 with
  standard-layout checksum/length inconsistencies, while 35 declared a
  section beyond that probe limit; those 35 are **not proven invalid**.
  These are research constraints, not evidence that the game data is corrupt.
- Of 174 standard-header candidates: 167 passed our strict *first-section*
  Adler32, header-size, version 1.0 and total-length checks. 7 need format
  investigation (length/checksum mismatch).
- In all 167 first-section-valid candidates, the next section is **uncompressed**
  and begins with object type 0 with a bounded object length. In strict JSR-184,
  object type 0 denotes a file header and must occur only in the first section.
  These files therefore do **not yet** pass full standard M3G validation.
- The strict native C++ `ds_port_m3g` container reader built and passed synthetic
  tests on Windows/Linux. When run against the real APK, it reported
  `unexpected second M3G header object` for those 167 assets, checksum mismatch
  on 7 and other signatures for 91. **Zero real full containers were accepted**.
  Do not describe the strict decoder as fully game-verified.
- Exactly 686 `.sph` and 686 `.sps` asset paths share 686 stems, confirming
  a consistent 1:1 paired resource naming pattern. Their internal layout
  remains unknown; no sprite/audio decoder is claimed.

## What is implemented

- `tools/deadspace_asset_recon.py`: redacted extension/sig census,
  includes JSR-184 and IronMonkey M3G identifiers, and paired resource counts.
- `tools/deadspace_m3g_preflight.py`: checks mandatory JSR-184 first sections,
  checksums, version/declared size, and summarizes the second-section layout.
- `include/ds_port/m3g_container.hpp` and `src/m3g_container.cpp`:
  bounds-limited, zlib-backed strict JSR-184 section + object-chunk reader.
- `apps/m3g_probe.cpp` and `tools/deadspace_m3g_native_verify.py`:
  native metadata-only checks of ephemeral game resources on the GitHub runner.
- `tests/m3g_container_tests.cpp` and Python synthetic regression fixtures.
- Reports are aggregate JSON metadata only; real game resource bytes are never
  part of builds, commits, releases, or published artifacts.

## Native texture decoder milestone — actual APK verified

- [GitHub Actions run 41289939920](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41289939920) confirmed that **all 167** JSR184-like assets pass bounded *diagnostic* section framing (not strict JSR184): 334 type-0 chunks and 167 type-10 (Image2D) chunks, exactly 2 sections per file. Strict parsing still rejects the extra type-0 header.
- **67 of 167** Image2D payloads pass the standard superclass/metadata/palette/pixel-byte checks, all immutable RGBA (format 100), maximum dimensions 1024×1024.
- The independent native C++ in-memory RGBA decoder succeeded for **all 67** genuine APK textures, returning **98,480,128 RGBA pixel bytes** in aggregate. The other **100** image payloads are still unsupported, not assumed to be broken.
- [Real APK research run 41290681546](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41290681546): the other **100** Image2D objects consistently carry format value **116 (0x74)**, not among JSR-184's standard 96–100 formats. Their Object3D base fields parsed sufficiently to locate the format byte, but the rest of the payload is not interpreted. Do **not** guess PVRTC, ETC, or a pixel layout solely from the code 116; identify its original ARM loader first.
- [GitHub Actions run 41290357084](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41290357084) compiled the new native PNG extractor, used it on **three** real textures, and validated PNG headers, CRC32s, dimensions, zlib decompression and scanline lengths. The PNG files were ephemeral and deleted; no copyrighted data entered artifacts.
- Local user-owned workflow: run `ds_asset_install` on the user's APK, then `ds_m3g_probe "<local texture.m3g>" --decode-image` and `ds_m3g_texture_extract "<local texture.m3g>" "<new output.png>"`. The extractor refuses to overwrite existing output.
- Code: `src/m3g_container.cpp` / `include/ds_port/m3g_container.hpp` decoder, `apps/m3g_texture_extract.cpp` PNG output, `tools/deadspace_m3g_native_verify.py` private APK checks, synthetic CTest fixtures. Windows and Linux builds use existing GitHub Actions, with no bundled game content.
- **Not yet achieved:** decoding the 100 other Image2D payloads, 91 IronMonkey meshes, renderer integration, gameplay engine, collision, AI, animation, audio, mission logic. No playable native PC build.
## Next format gate (do not bypass)

1. Compare `IM-M3G` first-section layout and compression/checksum with
   JSR-184, keeping dialects distinct.
2. For the JSR-184-header assets, investigate why **every** second section
   starts with object type 0; confirm the intended IronMonkey custom object
   structure against genuine native loader routines before relaxing strict
   validation. The strict parser must remain available as a baseline.
3. Implement a separately labeled IronMonkey structural parser, with bounded
   synthetic tests, and verify it against actual game files privately.
4. Only after object and texture layouts are verified, add a native OpenGL
   texture/mesh viewer; then animation, collision, engine behavior and missions.
   Green fixture CI is not evidence of playable game functionality.

**No native x86-64 playable game exists at this checkpoint.**
