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
 "0023e12c": (
     "Original physics world ray-test entry (called directly by PlasmaCutter::onFire)",
     'if(getenv("DS_AIM_DIAG")) { static unsigned n; if(++n<=120) fprintf(stderr,"[AIM_PHYSICS_WORLD_RAY] world=%08x from=%08x to=%08x callback=%08x count=%u\\n",r0,r1,r2,r3,n); }'
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
    # Crucial: PlasmaCutter::onFire does NOT call Weapon::performRaycast
    # for its actual shots. It directly calls original physics f_0023e12c
    # five times (three cutting beams plus additional ray checks), using the callback result at sp+28. Instrument those exact
    # call sites without changing the ray, filtering, or collision physics.
    fire_name = "u64 f_001f6344("
    ray_call = (
        "  { u64 rv = f_0023e12c(r0, r1, r2, r3, sp);"
        " r0 = (u32)rv; r1 = (u32)(rv>>32); }"
    )
    fire_matches=[]
    for f in files:
        source=changes.get(f)
        if source is None: source=f.read_text()
        begin=source.find(fire_name)
        if begin<0:continue
        finish=source.find("\nu64 f_", begin+len(fire_name))
        if finish<0:finish=len(source)
        fire_matches.append((f,source,begin,finish))
    if len(fire_matches)!=1:
        raise RuntimeError(f"PlasmaCutter::onFire found {len(fire_matches)} times")
    f,source,begin,finish=fire_matches[0]
    body=source[begin:finish]
    if body.count(ray_call)!=5:
        raise RuntimeError(
            f"Expected exactly 5 original plasma-cutter physics-ray calls, "
            f"found {body.count(ray_call)}; refusing to guess"
        )
    trace=(
        '  if(getenv("DS_AIM_DIAG")) { '
        'static unsigned n; ++n; '
        'fprintf(stderr,"[AIM_SHOT_RAY] shot_ray=%u world=%08x start=(%.5f,%.5f,%.5f) end=(%.5f,%.5f,%.5f) callback=%08x\\n",'
        'n,r0,u2f(LD32(r1)),u2f(LD32(r1+4u)),u2f(LD32(r1+8u)),'
        'u2f(LD32(r2)),u2f(LD32(r2+4u)),u2f(LD32(r2+8u)),r3); }\n'
        +ray_call+'\n'
        '  if(getenv("DS_AIM_DIAG")) fprintf(stderr,'
        '"[AIM_SHOT_RESULT] hit_object=%08x callback_result=%08x\\n",'
        'LD32(sp+28u),LD32(sp+20u));'
    )
    # Replace both native-authentic call sites; no modification of state.
    body=body.replace(ray_call,trace)
    changes[f]=source[:begin]+body+source[finish:]
    info.append("instrumented all five native PlasmaCutter physics-ray calls and result callbacks")
    for file,content in changes.items():file.write_text(content)
    return info
