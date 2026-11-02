#!/usr/bin/env python3
"""Verified, transactional local importer for the Vita RIP Xperia Play donor."""
import argparse
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

@dataclass(frozen=True)
class Donor:
    size: int
    sha256: str
    minimum_count: int
    minimum_bytes: int
    signatures: tuple

EXACT = Donor(
    145903794,
    "04a71b975d0a9aa81b0388aee080f927903507cfbebddc8b57b8959f3eae2d31",
    1700, 254000000, (
      ("lib/armeabi/libEAMGameDeadSpace.so", "1ff0a34a2ef35f50c0031b383d8fd7b14369dca0f41969c5d4da8fabe569ed38"),
      ("assets/EAMCore.ini", "d5b49184291c9a56928481380e19e8ead7f2df7b031201ffad52d8f374e05335"),
      ("assets/published/data/level_index.sb", "ef14959fea9519b6f698e153bc66cf0a4f9a49bff03d97f0f14b204136320636"),
      ("assets/published/layoutdata/layouts.xml.sb", "0f99badf5a7523ba0cc3e5749bf18b40807eb2d0fd11a66cc3c9c65656b4233a"),
      ("assets/published/models/main_menu.m3g", "58f88fed2d49276b7fc492ce2286f657820df3ba2b02cc01cda24c13cdd8d9e3"),
      ("assets/published/models/isaac.m3g", "fe287a58c55f7290dda080a0db43995bf0deb4c07c2580a509fafe87f33c3c02"),
      ("assets/published/textures/texture_isaac_suit_01.m3g", "c6e1c92745dfc2f75f6896cdb23a15fdc0f4041af349a25bc169ce3175c3294c"),
      ("assets/published/texturepacks_ui/ui_frontend.m3g", "0fa9c375a867dbeba56d34c28d2d4a0ea2cffdeb63869486be3ebc2bf08fa809"),
    )
)
PREFIX = "deadspace/"
LIB = "lib/armeabi/libEAMGameDeadSpace.so"
CHUNK=1024*1024

class DonorError(ValueError):
    pass

def digest(stream):
    result=hashlib.sha256()
    for chunk in iter(lambda: stream.read(CHUNK), b""):
        result.update(chunk)
    return result.hexdigest()

def check_names(zipfile):
    entries={}
    seen=set()
    if len(zipfile.infolist())>25000:
        raise DonorError("Excessive ZIP member count")
    for info in zipfile.infolist():
        name=info.filename
        if not name.startswith(PREFIX) or len(name)>512 or "\\" in name:
            raise DonorError("Unrecognized or unsafe ZIP path")
        tail=name[len(PREFIX):]
        if any(part in (".","..") or ":" in part for part in tail.split("/")):
            raise DonorError("Path traversal or host drive injection")
        if stat.S_ISLNK(info.external_attr>>16):
            raise DonorError("Symlinks are not accepted")
        if info.is_dir():continue
        if info.file_size>32*CHUNK:
            raise DonorError("Excessive individual asset size")
        if tail != LIB and not tail.startswith("assets/"):continue
        if tail.endswith(".DS_Store"):continue
        lowered=tail.casefold()
        if lowered in seen:raise DonorError("Duplicate or case-colliding asset")
        seen.add(lowered)
        entries[tail]=info
    if sum(i.file_size for i in entries.values())>350*CHUNK:
        raise DonorError("Archive expanded size exceeds safety cap")
    return entries

def verify(path,spec=EXACT):
    path=Path(path)
    if not path.is_file() or path.stat().st_size!=spec.size:
        raise DonorError("Archive size does not match known Vita RIP")
    with path.open("rb") as f:
        if digest(f)!=spec.sha256:
            raise DonorError("Archive SHA256 does not match known Vita RIP")
    with ZipFile(path) as z:
        entries=check_names(z)
        assets=[i for name,i in entries.items() if name.startswith("assets/")]
        total=sum(i.file_size for i in assets)
        if len(assets)<spec.minimum_count or total<spec.minimum_bytes:
            raise DonorError("Campaign asset set incomplete")
        for name,sha in spec.signatures:
            if name not in entries:
                raise DonorError("Required game resource missing: "+name)
            with z.open(entries[name]) as f:
                if digest(f)!=sha:
                    raise DonorError("Incorrect game edition: "+name)
        return {
            "schema":1, "profile":"vita-rip-atc",
            "asset_count":len(assets), "asset_bytes":total,
            "archive_sha256":spec.sha256,
            "rendering":"ATC textures (NOT the complete PVRTC donor)",
            "requires_arm_wrapper":True
        }

def install(source,destination,spec=EXACT,reanimated_apk=None,hd_spec=None):
    """Atomic combined installation. Never replace Xperia assets with HD ones."""
    report=verify(source,spec)
    # Verify BOTH user-owned archives before changing the filesystem.
    if reanimated_apk is not None:
        from tools.xperia_reanimated_hd_install import (
            HdSpec,inspect as verify_reanimated,install as install_reanimated)
        hd_spec=hd_spec or HdSpec()
        hd_report=verify_reanimated(reanimated_apk,hd_spec)
        report["reanimated_hd"]={
            "release":hd_report["release"],
            "resource_count":hd_report["hd_file_count"],
            "enabled_in_runtime":False,
            "compatibility":"sidecar; not yet mounted by original Xperia renderer"
        }
    else:
        report["reanimated_hd"]={"status":"not selected"}
    destination=Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise DonorError("Destination already exists; refusing overwrite")
    destination.parent.mkdir(parents=True,exist_ok=True)
    stage=Path(tempfile.mkdtemp(prefix=".xperia-stage-",dir=destination.parent))
    try:
        with ZipFile(source) as z:
            for name,item in check_names(z).items():
                target=stage.joinpath(*name.split("/"))
                target.parent.mkdir(parents=True,exist_ok=True)
                size=0
                with z.open(item) as src, target.open("xb") as dst:
                    while chunk:=src.read(CHUNK):
                        size+=len(chunk)
                        if size>item.file_size:
                            raise DonorError("ZIP expanded past declared size")
                        dst.write(chunk)
                if size!=item.file_size:
                    raise DonorError("Incomplete extracted asset")
        for name,sha in spec.signatures:
            with (stage/name).open("rb") as f:
                if digest(f)!=sha:raise DonorError("Post-install checksum mismatch")
        if reanimated_apk is not None:
            # Staged entirely beneath the not-yet-committed installation.
            # The Reanimated engine/older-edition assets are NEVER mixed
            # into /assets or /lib; only the HD texture sidecar is extracted.
            install_reanimated(
                reanimated_apk,stage/"enhancements"/"reanimated",hd_spec)
        (stage/".xperia-donor-info.json").write_text(
            json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf-8")
        if destination.exists() or destination.is_symlink():
            raise DonorError("Destination appeared while extracting")
        stage.rename(destination)
    finally:
        if stage.exists():shutil.rmtree(stage)
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zip",type=Path,help="Owned Xperia Play deadspace.zip")
    parser.add_argument("--reanimated-apk",type=Path,
                        help="Owned Reanimated v1.2 or v1.3 APK for HD textures")
    parser.add_argument("--no-hd",action="store_true",
                        help="Explicitly install original graphics only")
    parser.add_argument("--gui",action="store_true",
                        help="Open file selection dialogs for game ZIP and HD APK")
    parser.add_argument("--destination",type=Path)
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    if args.no_hd and args.reanimated_apk:
        parser.error("--no-hd and --reanimated-apk are mutually exclusive")

    # GUI file selection is optional, uses Python's standard Tk dialogs;
    # headless CI and explicit CLI arguments never start a window.
    if args.gui:
        try:
            import tkinter as tk
            from tkinter import filedialog
            ui=tk.Tk()
            ui.withdraw()
            if not args.zip:
                picked=filedialog.askopenfilename(
                    title="1 of 2 — Select Xperia Play Dead Space ZIP",
                    filetypes=[("ZIP archive","*.zip"),("All files","*.*")])
                if picked:args.zip=Path(picked)
            if not args.no_hd and not args.reanimated_apk:
                picked=filedialog.askopenfilename(
                    title="2 of 2 — Select Dead Space Reanimated APK (HD textures)",
                    filetypes=[("Android APK","*.apk"),("All files","*.*")])
                if picked:args.reanimated_apk=Path(picked)
            if not args.verify_only and not args.destination:
                parent=filedialog.askdirectory(
                    title="Select parent folder for new Dead Space Xperia installation")
                if parent:args.destination=Path(parent)/"DeadSpaceMobile-Xperia"
            ui.destroy()
        except (ImportError,RuntimeError) as err:
            parser.error("GUI file selector unavailable: "+str(err))
    elif sys.stdin.isatty():
        if args.zip is None:
            picked=input("Select Xperia Play deadspace.zip path: ").strip().strip('"')
            if picked:args.zip=Path(picked)
        if not args.no_hd and args.reanimated_apk is None:
            picked=input(
                "Select Dead Space Reanimated APK path for HD textures "
                "(Enter to skip HD): ").strip().strip('"')
            if picked:args.reanimated_apk=Path(picked)

    if args.zip is None:
        parser.error("Xperia deadspace.zip is required (--zip or --gui)")
    if not args.verify_only and args.destination is None:
        parser.error("--destination is required unless --verify-only is used")
    try:
        if args.verify_only:
            report=verify(args.zip)
            if args.reanimated_apk:
                from tools.xperia_reanimated_hd_install import inspect as inspect_hd
                report["reanimated_hd"]=inspect_hd(args.reanimated_apk)
        else:
            report=install(args.zip,args.destination,
                           reanimated_apk=args.reanimated_apk)
        print(json.dumps(report,indent=2,sort_keys=True))
        return 0
    except (OSError,ValueError) as err:
        print("XPERIA_DONOR_REJECTED:",err,file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
