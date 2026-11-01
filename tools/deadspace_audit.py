#!/usr/bin/env python3
"""Safe, read-only Dead Space Mobile source/APK inventory for GitHub runners.

Only JSON metadata is emitted. Proprietary assets and extracted libraries are
never copied into workflow artifacts.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from zipfile import ZipFile, BadZipFile

API_MARKERS = {
    "graphics": ("gl", "egl", "gles", "opengl", "vulkan"),
    "audio": ("opensl", "aaudio", "al_", "audio", "sound"),
    "input": ("touch", "motion", "keyevent", "input", "sensor"),
    "android_jni": ("jni", "android", "assetmanager", "nativeactivity"),
}
MACHINES = {3: "x86", 40: "ARM32", 62: "x86_64", 183: "AArch64"}


def scan_repo(root: Path) -> dict:
    counts = {}
    native = []
    for path in root.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        suffix = path.suffix.lower() or "[none]"
        counts[suffix] = counts.get(suffix, 0) + 1
        if suffix in (".so", ".a", ".dll", ".exe"):
            native.append(path.relative_to(root).as_posix())
    return {"files_by_suffix": dict(sorted(counts.items())),
            "native_file_paths": sorted(native)[:100]}


def command_output(*args: str) -> str:
    try:
        p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           text=True, timeout=30, check=False)
        return p.stdout if p.returncode == 0 else ""
    except (OSError, subprocess.TimeoutExpired):
        return ""



# Only known public platform API names are emitted. These are import evidence,
# NOT the game's internal functions or a proven texture/asset loader.
IMPORT_FAMILIES = {
    "android_asset_manager": (
        "AAssetManager_fromJava", "AAssetManager_open", "AAsset_open",
        "AAsset_read", "AAsset_seek", "AAsset_seek64", "AAsset_getBuffer",
        "AAsset_getLength", "AAsset_getLength64", "AAsset_close",
    ),
    "file_io": ("fopen", "fopen64", "fread", "fseek", "fseeko", "fclose",
                "open", "open64", "pread", "pread64", "mmap"),
    "decompression": ("inflate", "inflateInit2_", "uncompress",
                      "LZ4_decompress_safe", "ZSTD_decompress"),
    "gles_texture": ("glTexImage2D", "glCompressedTexImage2D",
                     "glTexSubImage2D", "glCompressedTexSubImage2D",
                     "glTexImage3D", "glCompressedTexImage3D"),
    "gles_draw": ("glDrawElements", "glDrawArrays", "glUseProgram",
                  "glVertexAttribPointer", "glBindBuffer"),
    "egl_context": ("eglCreateContext", "eglMakeCurrent", "eglSwapBuffers"),
    "audio": ("slCreateEngine", "alcOpenDevice", "alBufferData",
              "AMediaCodec_createDecoderByType"),
}
BOOTSTRAP_EXPORTS = {"JNI_OnLoad", "ANativeActivity_onCreate", "android_main"}
ELF_TYPES = {1: "ET_REL", 2: "ET_EXEC", 3: "ET_DYN"}


def parse_readelf_symbol_rows(output: str) -> tuple[set[str], set[str]]:
    """Read symbol Ndx/Name columns; omit addresses and all private names."""
    imported = set()
    exported = set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 8 or not fields[0].endswith(":"):
            continue
        name = fields[7].split("@", 1)[0]
        if not name:
            continue
        if fields[6] == "UND":
            imported.add(name)
        else:
            exported.add(name)
    return imported, exported


def public_api_evidence(imported: set[str]) -> dict:
    return {
        family: sorted(imported.intersection(symbols))
        for family, symbols in IMPORT_FAMILIES.items()
    }

def analyze_library(path: Path, archive_path: str) -> dict:
    # Metadata only; the ELF file exists solely in a temporary audit folder.
    data = path.read_bytes()[:64]
    result = {"apk_path": archive_path, "size_bytes": path.stat().st_size,
              "sha256": sha256_file(path)}
    if len(data) < 24 or data[:4] != b"\x7fELF":
        result["error"] = "Not an ELF library"
        return result
    if data[4] not in (1, 2) or data[5] not in (1, 2):
        result["error"] = "Invalid ELF class or endianness"
        return result
    is_64 = data[4] == 2
    if len(data) < (64 if is_64 else 52):
        result["error"] = "Truncated ELF header"
        return result
    order = "little" if data[5] == 1 else "big"
    e_type = int.from_bytes(data[16:18], order)
    machine = int.from_bytes(data[18:20], order)
    # ELF e_entry is not the Android game's update loop/JNI entrypoint.
    entry = int.from_bytes(data[24:32 if is_64 else 28], order)
    result.update({
        "elf_class": "ELF64" if is_64 else "ELF32",
        "elf_endianness": order,
        "elf_type": ELF_TYPES.get(e_type, f"unknown_{e_type}"),
        "machine": MACHINES.get(machine, f"unknown_{machine}"),
        "elf_entry_address_hex": hex(entry),
    })
    dyn = command_output("readelf", "-d", str(path))
    result["needed_libraries"] = sorted(
        set(re.findall(r"\(NEEDED\).*?\[([^]]+)\]", dyn))
    )
    symbols = command_output("readelf", "-Ws", str(path))
    imported, exported = parse_readelf_symbol_rows(symbols)
    result["undefined_symbol_count"] = len(imported)
    result["jni_export_hints"] = sorted(
        name for name in exported if name == "JNI_OnLoad" or name.startswith("Java_")
    )[:150]
    result["bootstrap_exports"] = sorted(exported & BOOTSTRAP_EXPORTS)
    # Only known platform API names; no private symbol table or text strings.
    result["platform_api_import_evidence"] = public_api_evidence(imported)
    result["import_hints"] = {
        key: sorted({name for name in imported
                     if any(marker in name.lower() for marker in markers)})[:100]
        for key, markers in API_MARKERS.items()
    }
    result["symbol_scan_available"] = bool(symbols.strip())
    result["evidence_limit"] = (
        "Public API imports/exports are clues to JNI, asset access and rendering; "
        "they do not identify original game loader functions or prove execution. "
        "A stripped or statically linked library may expose no relevant symbols."
    )
    return result


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def scan_apk(apk: Path) -> dict:
    report = {"apk_sha256": sha256_file(apk), "apk_size_bytes": apk.stat().st_size}
    try:
        with ZipFile(apk) as archive:
            entries = archive.infolist()
            library_entries = [item for item in entries
                               if item.filename.startswith("lib/") and item.filename.endswith(".so")
                               and not item.is_dir()]
            report["archive_entries"] = len(entries)
            report["archive_uncompressed_bytes"] = sum(item.file_size for item in entries)
            # Summaries contain counts/byte sizes only: never export asset filenames or bytes.
            by_group = {}
            for entry in entries:
                if entry.is_dir():
                    continue
                group = entry.filename.split("/", 1)[0] if "/" in entry.filename else "[root]"
                bucket = by_group.setdefault(group, {"files": 0, "uncompressed_bytes": 0})
                bucket["files"] += 1
                bucket["uncompressed_bytes"] += entry.file_size
            report["entry_groups"] = dict(sorted(by_group.items()))
            report["dex_count"] = sum(item.filename.endswith(".dex") for item in entries)
            report["native_libraries"] = []
            # Even a corrupt or malicious APK cannot trigger unbounded extraction.
            max_single_library_bytes = 256 * 1024 * 1024
            max_total_libraries_bytes = 1024 * 1024 * 1024
            used_bytes = 0
            with tempfile.TemporaryDirectory() as temp:
                for i, entry in enumerate(library_entries):
                    if (entry.file_size > max_single_library_bytes
                            or used_bytes + entry.file_size > max_total_libraries_bytes):
                        report["native_libraries"].append({
                            "apk_path": entry.filename,
                            "error": "Extraction skipped: native library exceeds audit size limit",
                            "declared_size_bytes": entry.file_size,
                        })
                        continue
                    used_bytes += entry.file_size
                    target = Path(temp) / f"library_{i}.so"
                    with archive.open(entry) as src, target.open("wb") as dst:
                        shutil.copyfileobj(src, dst)
                    report["native_libraries"].append(analyze_library(target, entry.filename))
            report["has_android_manifest"] = "AndroidManifest.xml" in archive.namelist()
            report["has_dex"] = report["dex_count"] > 0
    except BadZipFile:
        report["error"] = "APK is not a valid ZIP archive"
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--apk", type=Path)
    parser.add_argument("--output", type=Path, default=Path("report/deadspace-audit.json"))
    args = parser.parse_args()
    report = {"schema": 1, "purpose": "metadata-only desktop-port research",
              "repo": scan_repo(args.root.resolve()),
              "apk": scan_apk(args.apk) if args.apk else None,
              "limitations": [
                  "This does not translate ARM machine code to x86-64.",
                  "No native PC build is asserted by this report.",
                  "APK binaries are never uploaded as artifacts.",
              ]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote audit report to {args.output}")
    if args.apk is None:
        print("No APK provided: repository inventory only.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
