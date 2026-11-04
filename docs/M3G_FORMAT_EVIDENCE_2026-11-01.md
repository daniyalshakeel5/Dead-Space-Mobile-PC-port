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
- **Historical status, now superseded:** see later findings on native ETC1 and IM-M3G decoding. Renderer integration, gameplay, physics, audio, animation and missions remain missing.
## Superseding native decoder findings (actual APK verified)

**Code and original asset verification:**

- **Format 116 ETC1** — The code `0x74` refers to 100 immutable textures sharing a fixed `0x74` pixel start offset. **99/100** match ETC1 **full mipmap chains**, rather than base images alone; the remaining resource is still unsupported. New independent `src/etc1.cpp` and `include/ds_port/etc1.hpp` decode ETC1 base mip RGB into RGBA8 with bounded sizes and invalid-block rejection. Real APK native verification decoded **99** images (161,599,488 RGBA output bytes), with **three** temporary PNG smoke checks. **67 raw RGBA** images from the other format also decode. Neither family has visual-pixel-reference validation. [GitHub run 41291717211](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41291717211).
- **91 IM-M3G files** have bounded proprietary type/length object chunks. Observed: **4,154 Mesh**, **10,078 VertexArray**, **4,165 VertexBuffer**, **4,431 TriangleStripArray**, **4,431 proprietary IndexBuffer**, **11 SkinnedMesh** chunks. All 4,154 Mesh→VertexBuffer references, all 4,165 VertexBuffer→VertexArray references and 10,078 exact VertexArray raw sizes passed. Index encodings `0x81` (3,722) and `0x82` (709); all IndexBuffer→strip/Appearance references valid. [Topology run 41292317869](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41292317869).
- **Verified geometry numerics** — All 4,154 meshes use signed int16×3 position arrays and finite VertexBuffer bias/scale; all 4,362 referenced index buffers use exactly three indices per strip with every index in bounds. [Numeric run 41292464520](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41292464520).
- **Native model extractor** — `src/im_mesh.cpp`, `include/ds_port/im_mesh.hpp`, `apps/im_mesh_extract.cpp` and `tests/im_mesh_tests.cpp` add bounded C++17 position/triangle OBJ extraction. **37/91 genuine IM-M3G resources** decoded: **4,154 Mesh objects, 4,362 submeshes, 546,488 vertices, 398,634 triangles**. Three temporary original-model OBJ exports passed face/vertex/index count verification and were deleted. The other 54 resources are outside this supported geometry subset, not necessarily corrupt. [Native real-APK CI 41292859630](https://github.com/daniyalshakeel5/Dead-Space-Mobile-PC-port/actions/runs/41292859630).
- This geometry output has **no UVs, normals, textures, materials, rigging, animations, scene transforms or demonstrated visual fidelity**. The proprietary IM-M3G trailer/checksum is not original-native validated. No playable engine or OpenGL game scene exists.
- Credits: [Sleepyzay's Dead Space Mobile Noesis/Maxscript research](https://github.com/sleepyzay/Maxscript-Projects/tree/main/Dead%20Space%20Mobile), [RaduMC/M3G2FBX](https://github.com/RaduMC/M3G2FBX), [ResHax format discussion](https://reshax.com/topic/1460-mobile-dead-space-m3g-modeltexture-bundles-help/), original mobile fixes [RAFALAMAO](https://github.com/RAFALAMAO/Dead-Space-Mobile-Fixes). Independent code does not include copied game or community tool assets.

**Local usage on user-owned resources only:**

```sh
ds_m3g_texture_extract "<owned-local-texture.m3g>" "<new-output.png>"
ds_im_mesh_extract "<owned-local-model.m3g>"               # metadata only
ds_im_mesh_extract "<owned-local-model.m3g>" "<new-output.obj>"
```

**Next work:** verify UV/reference material/normal data in the original IM-M3G assets; integrate verified untextured geometry in an SDL2/OpenGL **diagnostic** viewer; then validate coordinate orientation, material/texture bindings, animation and original engine behavior. All unimplemented systems must stay labeled as such. No emulation, no proprietary game asset distribution, no use of excluded milestone ZIP.

