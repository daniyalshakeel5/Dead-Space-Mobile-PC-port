#!/usr/bin/env python3
"""Bounded splash/menu inventory of the user's owned original APK.

Only reports asset path and size metadata. Does not extract game files,
images, copyrighted data, or ELF instructions.
"""
from __future__ import annotations
import argparse
import collections
import json
from pathlib import PurePosixPath
import re
import zipfile

KEYWORDS = re.compile(r"(splash|title|logo|menu|frontend|front.?end|intro|boot|startup|loading|button|hud|gui|ui|ea[_./-]|ironmonkey)", re.I)
SUFFIXES = {".png",".jpg",".jpeg",".bmp",".tga",".webp",".m3g",".sph",".sps",".xml",".json",".atlas",".font",".fnt",".sb",".txt"}
def inventory(archive: zipfile.ZipFile) -> dict:
    matches=[]
    suffix_counts=collections.Counter()
    counts=collections.Counter()
    for item in archive.infolist():
        if item.is_dir():continue
        path=item.filename
        if not path.startswith(("assets/","res/")):continue
        if len(path)>512 or item.file_size<0:continue
        ext=PurePosixPath(path).suffix.lower()
        suffix_counts[ext or "(none)"]+=1
        if KEYWORDS.search(path):
            matches.append({
                "path":path,"size":item.file_size,
                "extension":ext or "(none)"})
            counts[ext or "(none)"]+=1
    matches.sort(key=lambda row:(row["path"].casefold(),row["path"]))
    return {
        "asset_extension_counts":dict(sorted(suffix_counts.items())),
        "candidate_extension_counts":dict(sorted(counts.items())),
        "candidate_count":len(matches),
        "candidate_paths":matches[:300],
        "candidate_paths_truncated":len(matches)>300,
    }
def main() -> int:
    parser=argparse.ArgumentParser()
    parser.add_argument("--apk",required=True)
    args=parser.parse_args()
    with zipfile.ZipFile(args.apk) as apk:
        report=inventory(apk)
    print("ORIGINAL_BOOT_ASSET_MANIFEST",json.dumps(report,separators=(",",":")))
    return 0
if __name__=="__main__":raise SystemExit(main())
