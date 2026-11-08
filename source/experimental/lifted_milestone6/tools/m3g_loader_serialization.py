#!/usr/bin/env python3
"""Serialize original native M3G loader against concurrent real game threads.

Original ARM-translated function remains verbatim and always runs; only a
native recursive mutex is added when DS_M3G_SERIALIZE=1. Never creates a
placeholder model or suppresses an asset-decoder error.
"""
import argparse
from pathlib import Path
import sys

ORIG="u64 f_0032aed4(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
NEW="static u64 ds_original_m3g_stream_loader(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {"
MARK="DS_ORIGINAL_M3G_STREAM_LOCK"
WRAPPER="""
/* DS_ORIGINAL_M3G_STREAM_LOCK; real ARM implementation runs unchanged. */
static pthread_mutex_t ds_m3g_lock;
static pthread_once_t ds_m3g_once = PTHREAD_ONCE_INIT;
static void ds_m3g_lock_init(void) {
    pthread_mutexattr_t at;
    pthread_mutexattr_init(&at);
    pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&ds_m3g_lock, &at);
    pthread_mutexattr_destroy(&at);
}
u64 f_0032aed4(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    if (!getenv("DS_M3G_SERIALIZE"))
        return ds_original_m3g_stream_loader(r0,r1,r2,r3,sp);
    pthread_once(&ds_m3g_once,ds_m3g_lock_init);
    pthread_mutex_lock(&ds_m3g_lock);
    u64 result=ds_original_m3g_stream_loader(r0,r1,r2,r3,sp);
    pthread_mutex_unlock(&ds_m3g_lock);
    return result;
}
"""

def patch(s):
    if MARK in s:
        if s.count(MARK)!=1: raise ValueError("Native lock marker repeated")
        return s
    if s.count(ORIG)!=1 or "Loader4load" not in s:
        raise ValueError("Original ARM M3G loader function changed")
    return "#include <pthread.h>\n#include <stdlib.h>\n"+s.replace(ORIG,NEW)+WRAPPER

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source",required=True,type=Path)
    ap.add_argument("--instrument",action="store_true")
    a=ap.parse_args()
    try:
        old=a.source.read_text(encoding="utf8")
        new=patch(old)
        if a.instrument:
            bak=a.source.with_suffix(".c.pre-m3g-serialization.bak")
            if not bak.exists():bak.write_text(old,encoding="utf8")
            a.source.write_text(new,encoding="utf8")
            print("[REAL_M3G] Original native loader serialized when DS_M3G_SERIALIZE is set")
        else:print("[REAL_M3G] Original ARM native decoder validated (read-only)")
        return 0
    except (OSError,ValueError) as e:
        print("[REAL_M3G SERIAL ERROR]",e,file=sys.stderr);return 2
if __name__=="__main__":raise SystemExit(main())
