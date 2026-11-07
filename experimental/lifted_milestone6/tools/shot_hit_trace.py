#!/usr/bin/env python3
"""Opt-in diagnostics at native-verified original game call sites.

No gameplay values or hitboxes are modified. DS_AIM_DIAG=1 records world-space ray endpoints, collision-result pointers, and when the
original camera is assigned an aspect ratio, when projectile simulation runs,
and when an original shoot-switch hit callback is *actually* reached.
These RVAs were looked up in the authentic 2011 ELF (not guessed).
"""
from pathlib import Path

PROBES = {
 "0003dd14": (
     "CameraController::setAspect",
     'if(getenv("DS_AIM_DIAG")) fprintf(stderr,"[AIM_CAMERA_ASPECT] this=%08x aspect=%.8f\\n",r0,u2f(r1));'
 ),
 "0013264c": (
     "GameObjectShootSwitch::onHit",
     'if(getenv("DS_AIM_DIAG")) fprintf(stderr,"[AIM_FUSE_HIT] ShootSwitch this=%08x hit=%08x event_type=%u\\n",r0,r1,r1?LD32(r1+84u):0);'
 ),
 "00133278": (
     "GameObjectShootSwitchCovered::onHit",
     'if(getenv("DS_AIM_DIAG")) fprintf(stderr,"[AIM_FUSE_HIT] CoveredShootSwitch this=%08x hit=%08x event_type=%u\\n",r0,r1,r1?LD32(r1+84u):0);'
 ),
 "00174834": (
     "GameObjectProjectile::updateAction",
     'if(getenv("DS_AIM_DIAG")) { static unsigned n; if(++n<=8 || n%500==0) fprintf(stderr,"[AIM_PROJECTILE_SIM] this=%08x world=%08x n=%u\\n",r0,r1,n); }'
 ),
 "001f6344": (
     "WeaponPlasmaCutter::onFire",
     'if(getenv("DS_AIM_DIAG")) fprintf(stderr,"[AIM_FIRE] plasma-cutter onFire weapon=%08x\\n",r0);'
 ),
 "001eec34": (
     "Weapon::performRaycast",
     'if(getenv("DS_AIM_DIAG")) { fprintf(stderr,"[AIM_RAYCAST] result=%08x from=%08x to=%08x\\n",r0,r1,r2); if(r1 && r2) fprintf(stderr,"[AIM_RAY_SEGMENT] origin=(%.4f,%.4f,%.4f) endpoint=(%.4f,%.4f,%.4f)\\n",u2f(LD32(r1+0u)),u2f(LD32(r1+4u)),u2f(LD32(r1+8u)),u2f(LD32(r2+0u)),u2f(LD32(r2+4u)),u2f(LD32(r2+8u))); }'
 ),
 "001ee630": (
     "Weapon::WeaponRayResult::addSingleResult",
     'if(getenv("DS_AIM_DIAG")) { static unsigned n; if(++n<=200) fprintf(stderr,"[AIM_RAY_HIT] callback=%08x local-result=%08x collision-word0=%08x n=%u\\n",r0,r1,r1?LD32(r1):0,n); }'
 ),
 "001ef110": (
     "Weapon::resolveImpact",
     'if(getenv("DS_AIM_DIAG")) fprintf(stderr,"[AIM_IMPACT] weapon=%08x target=%08x hit-vector=%08x\\n",r0,r1,r2);'
 ),
 "001f6ea4": (
     "WeaponPlasmaCutter::onPostHitEvent",
     'if(getenv("DS_AIM_DIAG")) fprintf(stderr,"[AIM_POST_HIT] plasma cutter=%08x hit=%08x\\n",r0,r1);'
 ),
}

def patch(out: Path) -> list[str]:
    files=sorted(out.glob("fn_*.c"))
    if not files: raise RuntimeError("No genuine translated function bodies")
    changes: dict[Path,str]={}
    info=[]
    for rva,(name,body) in PROBES.items():
        target=f"u64 f_{rva}("
        matches=[]
        for f in files:
            content=changes.get(f)
            if content is None: content=f.read_text()
            at=content.find(target)
            if at<0: continue
            if content.find(target,at+len(target))!=-1: raise RuntimeError("Duplicate "+target)
            matches.append((f,content,at))
        if len(matches)!=1: raise RuntimeError(f"{name} ({target}) appears {len(matches)} times, expected exactly one")
        f,content,start=matches[0]
        brace=content.find("{",start+len(target))
        if brace<0 or brace>start+220: raise RuntimeError("Unexpected function header for "+name)
        probe='  '+body+'\n'
        if content[brace+1:].startswith('\n'+probe):
            info.append("already patched "+name); continue
        content=content[:brace+1]+'\n'+probe+content[brace+1:]
        changes[f]=content
        info.append(f"instrumented {name} at RVA 0x{rva} in {f.name}")
    for file,content in changes.items():file.write_text(content)
    return info
