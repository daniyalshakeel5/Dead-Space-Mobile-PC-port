#!/usr/bin/env python3
"""Opt-in x86-64 PC boot via EapRules' real GPL ARM loader and qemu-arm.
Requires Docker Linux containers, original Xperia assets kept local.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

UPSTREAM="https://github.com/EapRules/deadspace-native-arm.git"
REV="cc8fd2c8a16530936aa3c2ce205ef4cffc2814e9"
IMAGE="xperia-arm-linux-verified"
SHA1="0ed42b611415015807f759ec9b5457857143ce39"
ASSETS=("assets/EAMCore.ini","assets/published/data/level_index.sb",
        "assets/published/models/main_menu.m3g",
        "assets/published/texturepacks_ui/ui_frontend.m3g")
class BootError(RuntimeError):pass

def verify_game(folder):
    if not folder.is_dir():raise BootError("Installed game folder missing")
    lib=folder/"lib/armeabi/libEAMGameDeadSpace.so"
    if not lib.is_file() or lib.stat().st_size!=4499468:
        raise BootError("Wrong Xperia Play original ELF")
    sha=hashlib.sha1()
    with lib.open("rb") as f:
        while part:=f.read(1024*1024):sha.update(part)
    if sha.hexdigest()!=SHA1:raise BootError("Unsupported original game revision")
    missing=[n for n in ASSETS if not (folder/n).is_file()]
    if missing:raise BootError("Missing original game resources: "+", ".join(missing))
    return {"verified":True,"game":"Xperia Play v1.1.33","asset_markers":len(ASSETS)}

def milestones(s,code):
    def last(pattern):
        hits=re.findall(pattern,s)
        return int(hits[-1]) if hits else 0
    module="TRACE: module loaded" in s
    started=module and "TRACE: JNI_OnLoad returned" in s and "TRACE: NativeOnCreate returned" in s
    surface="TRACE: surface created" in s
    frames=last(r"TRACE: frames=(\d+)")
    triples=re.findall(r"TRACE: summary assets=(\d+) textures=(\d+) draws=(\d+)",s)
    a,t,d=map(int,triples[-1]) if triples else (0,0,0)
    changes=re.findall(r"TRACE: autopilot keys=(\d+) scenes=(\d+)",s)
    keys,scenes=map(int,changes[-1]) if changes else (0,0)
    stage=2 if module else 0
    if started:stage=3
    if stage==3 and surface:stage=4
    if stage==4 and frames>=60:stage=5
    if stage==5 and "TRACE: framebuffer non-black" in s and a>=50 and t>=10 and d>=100:stage=6
    if stage==6 and keys>=1 and scenes>=2:stage=7
    return {"milestone":stage,"real_game_started":started,
            "frames":frames,"assets_opened":a,"textures_uploaded":t,
            "draw_calls":d,"input_events":keys,"scene_changes":scenes,
            "exit_code":code,"backend":"qemu-arm + EapRules GPL ARM loader"}

def commands(source,game,frames,seconds):
    source=str(source.resolve());game=str(game.resolve())
    image=["docker","build","-t",IMAGE,"-f","Dockerfile.build",source]
    arm_build=["docker","run","--rm","-v",source+":/src","-w","/src",IMAGE,"make","-j4"]
    play=["docker","run","--rm","-v",source+":/src","-v",game+":/game:ro",
          "-w","/src","-e","SDL_VIDEODRIVER=offscreen",
          "-e","SDL_AUDIODRIVER=dummy","-e","LIBGL_ALWAYS_SOFTWARE=1",
          "-e","GALLIUM_DRIVER=llvmpipe","-e","EGL_PLATFORM=surfaceless",
          "-e","LOADER_TRACE=1","-e","DEADSPACE_AUTOPILOT=1",
          "-e","DEADSPACE_FRAME_LIMIT="+str(frames),
          IMAGE,"timeout","--signal=INT",str(seconds),
          "qemu-arm","-L","/usr/arm-linux-gnueabihf","./build/deadspace","/game"]
    return image,arm_build,play

def prepare_source(source):
    if not source.exists():
        if not shutil.which("git"):raise BootError("Git is required for GPL source checkout")
        source.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(["git","clone","--no-checkout",UPSTREAM,str(source)],check=True)
        subprocess.run(["git","-C",str(source),"checkout","--detach",REV],check=True)
    head=subprocess.run(["git","-C",str(source),"rev-parse","HEAD"],
                        check=True,capture_output=True,text=True).stdout.strip()
    if head!=REV:raise BootError("External GPL source checkout is not pinned")
    for file in ("Dockerfile.build","Makefile","emulator/run.sh"):
        if not (source/file).is_file():raise BootError("Missing GPL loader build file: "+file)

def run_logged(argv,timeout,path):
    proc=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                        text=True,errors="replace",timeout=timeout,check=False)
    path.write_text(proc.stdout[:8_000_000],encoding="utf-8")
    return proc.returncode

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--game-dir",required=True,type=Path)
    p.add_argument("--source-dir",type=Path,default=Path.home()/".cache/deadspace-xperia/gpl-loader")
    p.add_argument("--logs",type=Path,default=Path("xperia-boot-diagnostics"))
    p.add_argument("--frames",type=int,default=570)
    p.add_argument("--seconds",type=int,default=150)
    p.add_argument("--allow-external-build",action="store_true")
    p.add_argument("--dry-run",action="store_true")
    a=p.parse_args()
    try:
        verified=verify_game(a.game_dir)
        if not (60<=a.frames<=30000 and 30<=a.seconds<=1800):
            raise BootError("Invalid bounded frame/time settings")
        stages=commands(a.source_dir,a.game_dir,a.frames,a.seconds)
        if a.dry_run:
            print(json.dumps({"input":verified,"executed":False,"commands":stages},indent=2))
            return 0
        if not a.allow_external_build:raise BootError("Explicit --allow-external-build is required")
        if not shutil.which("docker"):raise BootError("Docker Linux containers unavailable")
        prepare_source(a.source_dir)
        a.logs.mkdir(parents=True,exist_ok=True)
        for i,label in enumerate(("image-build","arm-loader-build","real-game")):
            rc=run_logged(stages[i],1200 if i<2 else a.seconds+60,a.logs/(label+".log"))
            if i<2 and rc:raise BootError(label+" failed; inspect local log")
        output=(a.logs/"real-game.log").read_text(encoding="utf-8",errors="replace")
        result=milestones(output,rc)
        result["game"]=verified
        (a.logs/"boot-status.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
        print(json.dumps(result,indent=2))
        return 0 if result["real_game_started"] else 3
    except (BootError,OSError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as e:
        print("XPERIA_BOOT_ERROR:",e,file=sys.stderr)
        return 2
if __name__=="__main__":raise SystemExit(main())
