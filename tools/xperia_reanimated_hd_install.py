#!/usr/bin/env python3
"""Verify/install user-provided Dead Space Reanimated HD textures only.

This is a separate, unmounted sidecar resource layer.  The Xperia Play
ARM game and its ATC assets must never be replaced with this newer-edition
remaster's files. No game files are distributed or uploaded.
"""
from __future__ import annotations
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import shutil
import stat
import sys
import tempfile
from zipfile import ZipFile

MIB = 1024 * 1024
CHUNK = MIB
HD_PREFIX = "assets/texture_hd/"
RELEASES = {
    # From GitHub release API, v1.2 and v1.3 are distinct APK contents.
    "v1.2": "042c0d60c53c72d3c71137710834f2081684625d66d0c3d214d9e75d468830a3",
    "v1.3": "dbd94ee54fc3fefdddb101fac4eba1e2058a5db7cfcf30ac41ad96e88b285a3a",
}
RELEASE_BYTES = 715366829

class HdAssetError(ValueError):
    pass

@dataclass(frozen=True)
class HdSpec:
    size: int = RELEASE_BYTES
    accepted_hashes: tuple[str,...] = tuple(RELEASES.values())
    min_textures: int = 200
    max_textures: int = 320
    max_total: int = 1536 * MIB
    max_file: int = 256 * MIB

def sha256_stream(file) -> str:
    sha=hashlib.sha256()
    for block in iter(lambda: file.read(CHUNK), b""):
        sha.update(block)
    return sha.hexdigest()

def _safe_hd_entries(archive: ZipFile,spec: HdSpec):
    if len(archive.infolist())>50000:
        raise HdAssetError("Too many APK ZIP entries")
    files={}
    seen=set()
    total=0
    for entry in archive.infolist():
        name=entry.filename
        if not name.startswith(HD_PREFIX):
            continue
        rel=name[len(HD_PREFIX):]
        if entry.is_dir():
            continue
        if not rel or len(rel)>512 or any(c in rel for c in "\\\0") or rel.startswith("/"):
            raise HdAssetError("Unsafe Reanimated HD filename")
        parts=rel.split("/")
        if any(part in ("", ".", "..") or ":" in part for part in parts):
            raise HdAssetError("Reanimated HD path traversal or drive name")
        if stat.S_ISLNK(entry.external_attr>>16):
            raise HdAssetError("Reanimated HD symlinks are unsupported")
        if entry.file_size<=0 or entry.file_size>spec.max_file:
            raise HdAssetError("Invalid HD resource size")
        folded=rel.casefold()
        if folded in seen:
            raise HdAssetError("Duplicate Reanimated HD resource")
        seen.add(folded)
        total+=entry.file_size
        if total>spec.max_total:
            raise HdAssetError("Reanimated HD resources exceed install cap")
        files[rel]=entry
    if not spec.min_textures<=len(files)<=spec.max_textures:
        raise HdAssetError("Reanimated APK has an unexpected HD resource count")
    return files,total

def inspect(apk: Path,spec: HdSpec=HdSpec()) -> dict:
    apk=Path(apk)
    if not apk.is_file() or apk.stat().st_size!=spec.size:
        raise HdAssetError("Not a supported Reanimated v1.2/v1.3 APK size")
    with apk.open("rb") as source:
        sha=sha256_stream(source)
    if sha not in spec.accepted_hashes:
        raise HdAssetError("Unsupported Reanimated APK SHA256 (v1.2/v1.3 only)")
    with ZipFile(apk) as archive:
        files,total=_safe_hd_entries(archive,spec)
        types=dict(sorted(Counter(
            Path(rel).suffix.lower() or "[none]" for rel in files
        ).items()))
    release=next((name for name,h in RELEASES.items() if h==sha),"synthetic-test")
    return {
        "schema":1,"source":"DeadSpaceReanimated",
        "release":release,"apk_sha256":sha,
        "hd_file_count":len(files),"hd_uncompressed_bytes":total,
        "extensions":types,
        "asset_layer":"texture_hd",
        "mounted_in_game":False,
        "compatibility":"separate edition; runtime override mapping not verified",
    }

def install(apk: Path,destination: Path,spec: HdSpec=HdSpec()) -> dict:
    """Install HD-only sidecar atomically. Never overwrite original assets."""
    report=inspect(apk,spec)
    destination=Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise HdAssetError("HD destination exists: refusing to overwrite")
    destination.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix=".reanimated-hd-",dir=destination.parent))
    try:
        with ZipFile(apk) as archive:
            files,_=_safe_hd_entries(archive,spec)
            for rel,info in files.items():
                target=stage/"texture_hd"/Path(*rel.split("/"))
                target.parent.mkdir(parents=True,exist_ok=True)
                copied=0
                with archive.open(info) as src,target.open("xb") as dst:
                    while block:=src.read(CHUNK):
                        copied+=len(block)
                        if copied>info.file_size:
                            raise HdAssetError("HD resource expanded beyond declared size")
                        dst.write(block)
                if copied!=info.file_size:
                    raise HdAssetError("HD resource truncated")
        (stage/"hd-layer.json").write_text(
            json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
        if destination.exists() or destination.is_symlink():
            raise HdAssetError("HD destination appeared during installation")
        stage.rename(destination)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    return report

def main() -> int:
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk",type=Path,required=True)
    parser.add_argument("--destination",type=Path)
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    if not args.verify_only and args.destination is None:
        parser.error("Specify --destination or --verify-only")
    try:
        info=inspect(args.apk) if args.verify_only else install(args.apk,args.destination)
        print(json.dumps(info,indent=2,sort_keys=True))
        return 0
    except (OSError,ValueError) as e:
        print("REANIMATED_HD_REJECTED:",e,file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
