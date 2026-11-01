#!/usr/bin/env python3
"""Bounded original ARM instruction evidence for native-code reconstruction.

Reads the user's confirmed APK only on ephemeral GitHub runners. Reports short
instruction windows at verified native JNI/engine control-flow addresses.
Never publishes ELF bytes, strings, decompiled C or a complete disassembly.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path
from zipfile import ZipFile

# Actual addresses independently grounded by previous native Ghidra/ELF runs.
# Original *ELF virtual addresses*. Do not pass Ghidra's re-based values
# (+0x10000 for this import) to an ELF section/segment mapper.
WINDOWS = {
    "jni_draw_frame_branch": (0x0027f008, 4),
    "native_frame_state_dispatch": (0x0027ef68, 144),
    "original_game_entry_jni": (0x00035de8, 48),
    "game_entry_direct_callee": (0x0027e974, 12),
    "activity_native_create": (0x0027ee54, 64),
    "renderer_native_surface_changed": (0x0027ecf8, 64),
    "renderer_native_surface_created": (0x0027ec24, 84),
    "frame_stage_zero_routine": (0x0027f870, 24),
    "frame_stage_zero_first_callee": (0x0027f790, 80),
    "frame_stage_zero_second_callee": (0x00280520, 96),
    "stage_zero_setup_target_0x28ca24": (0x0028ca24, 160),
    "stage_zero_setup_type2_branch_0x28cad8": (0x0028cad8, 96),
    "stage_zero_followup_target_0x281d54": (0x00281d54, 160),
    "stage_zero_followup_target_0x2833f4": (0x002833f4, 160),
    "original_time_pair_source": (0x000357c4, 12),
    "frame_stage_one_routine": (0x0027fc68, 96),
    "frame_state_one_zero_result_tail": (0x0027eecc, 128),
    "frame_state_one_nonzero_continuation": (0x0027efc8, 80),
}


def elf32_load_ranges(raw: bytes) -> list[tuple[int,int,int]]:
    if len(raw)<52 or raw[:6]!=b"\x7fELF\x01\x01":
        raise ValueError("Original native binary is not little-endian ELF32")
    phoff=struct.unpack_from("<I",raw,28)[0]
    phsize,phcount=struct.unpack_from("<HH",raw,42)
    if phsize<32 or phcount>1024 or phoff>len(raw) or phsize*phcount>len(raw)-phoff:
        raise ValueError("Unsafe ELF program header table")
    segments=[]
    for i in range(phcount):
        typ,offset,va,_,filesz,memsz,flags,_=struct.unpack_from(
            "<IIIIIIII",raw,phoff+i*phsize)
        if typ!=1: continue
        if filesz>memsz or offset>len(raw) or filesz>len(raw)-offset:
            raise ValueError("Unsafe PT_LOAD segment")
        if filesz:segments.append((va,va+filesz,offset))
    if not segments:raise ValueError("No ELF PT_LOAD segments")
    return segments


def bounded_virtual_read(raw: bytes, segs: list[tuple[int,int,int]],
                         va: int, count: int) -> bytes:
    if count<1 or count>256: raise ValueError("Unsafe instruction window size")
    for start,end,offset in segs:
        if start<=va and va+count<=end:
            p=offset+(va-start)
            return raw[p:p+count]
    raise ValueError("Instruction address not covered by file-backed PT_LOAD")


def elf32_executable_sections(raw: bytes) -> list[tuple[int,int,int]]:
    """Map SHF_EXECINSTR SHT_PROGBITS sections independently of PT_LOAD."""
    if len(raw)<52:raise ValueError("ELF too short")
    shoff=struct.unpack_from("<I",raw,32)[0]
    shsize,shcount=struct.unpack_from("<HH",raw,46)
    if shsize<40 or shcount>4096 or shoff>len(raw) or shsize*shcount>len(raw)-shoff:
        raise ValueError("Unsafe section header table")
    sections=[]
    for i in range(shcount):
        _,typ,flags,addr,off,size,_,_,_,_=struct.unpack_from(
            "<IIIIIIIIII",raw,shoff+i*shsize)
        if typ!=1 or not(flags & 4):continue
        if off>len(raw) or size>len(raw)-off:
            raise ValueError("Executable ELF section outside file")
        if size:sections.append((addr,addr+size,off))
    if not sections:raise ValueError("No executable sections in ARM ELF")
    return sections


def mapping_comparison(raw: bytes, sections: list[tuple[int,int,int]],
                       segments: list[tuple[int,int,int]],
                       va: int, nbytes: int) -> dict:
    section=bounded_virtual_read(raw,sections,va,nbytes)
    segment_candidates=[]
    for span in segments:
        if span[0]<=va and va+nbytes<=span[1]:
            data=bounded_virtual_read(raw,[span],va,nbytes)
            segment_candidates.append(data==section)
    return {
        "executable_section_present": True,
        "segment_candidate_count": len(segment_candidates),
        "any_segment_matches_section": any(segment_candidates),
        "all_segments_match_section": all(segment_candidates)
    }


def resolve_arm_literal_based_global(raw: bytes, sections: list[tuple[int,int,int]],
                                     ldr_addr: int, ldr_pc_immediate: int,
                                     add_addr: int) -> int:
    """ARM32 LDR reg,[PC,#imm] followed by ADD reg,PC,reg.

    PC at each ARM instruction is instruction address + 8, and the
    32-bit immediate loaded from the executable literal pool is signed.
    Input addresses/offsets must first be verified against the game's ARM
    instructions. Return the referenced *virtual address*, never data.
    """
    literal_at=ldr_addr+8+ldr_pc_immediate
    displacement=struct.unpack("<i",bounded_virtual_read(
        raw,sections,literal_at,4))[0]
    return (add_addr+8+displacement)&0xffffffff


def prove_runtime_global_layout(frame_global: int, record_slot: int,
                                type2_float: int) -> dict:
    """Verify the three exact original ELF addresses before host mapping."""
    if (frame_global != 0x0049caa8 or
        record_slot-frame_global != 0xec or
        type2_float-frame_global != 0x274):
        raise ValueError("Original ARM globals do not match native port offsets")
    return {
        "base":f"0x{frame_global:08x}",
        "record_slot_relative_offset":0xec,
        "type2_float_relative_offset":0x274,
        "minimum_consecutive_span":0x278,
    }


def elf32_jni_symbols(raw: bytes, names: set[str]) -> dict[str, tuple[int, int, bool]]:
    """Use original ELF dynamic symbols, not Ghidra's inferred function starts."""
    shoff=struct.unpack_from("<I",raw,32)[0]
    shsize,count=struct.unpack_from("<HH",raw,46)
    if shsize<40 or count>4096 or shoff>len(raw) or shsize*count>len(raw)-shoff:
        raise ValueError("Invalid original section table")
    sections=[]
    for i in range(count):
        fields=struct.unpack_from("<IIIIIIIIII",raw,shoff+i*shsize)
        _,typ,_,addr,offset,length,link,_,_,step=fields
        if typ!=8 and (offset>len(raw) or length>len(raw)-offset):
            raise ValueError("Corrupt section bounds")
        sections.append((typ,addr,offset,length,link,step))
    found={}
    for typ,_,offset,length,link,step in sections:
        if typ!=11 or link>=len(sections) or step<16 or step>128:
            continue
        _,_,name_offset,name_size,_,_=sections[link]
        for at in range(offset,offset+length,step):
            if at+16>offset+length:break
            name_off,value,size,attr,_,sect=struct.unpack_from("<IIIBBH",raw,at)
            if name_off>=name_size or (attr&15)!=2 or not size or sect>=len(sections):
                continue
            begin=name_offset+name_off
            end=raw.find(b"\x00",begin,name_offset+name_size)
            if end<0 or end-begin>240:continue
            name=raw[begin:end].decode("ascii","ignore")
            if name not in names:continue
            sym=(value&~1,size,bool(value&1))
            if name in found and found[name]!=sym:
                raise ValueError("Conflicting ELF symbol values")
            found[name]=sym
    return found


def proven_frame_dispatch_prefix(insns: list[dict]) -> dict[str, bool]:
    """Prove short original ARM frame state transitions by exact instructions.

    No inference from JNI names; a changed binary must NOT quietly pass.
    These checks define the precise behaviors used by original_frame_dispatch.cpp.
    """
    selected={item["offset"]:(item["op"],item["args"]) for item in insns}
    frame_global_literal={
        4:("ldr","r4, [pc, #0x90]"),
        12:("add","r4, pc, r4"),
    }
    stage_selectors={
        16:("ldr","r3, [r4]"),
        20:("cmp","r3, #0"),
        24:("beq","#0x27ef98"),
        28:("ldr","r3, [r4]"),
        32:("cmp","r3, #1"),
        36:("beq","#0x27efac"),
        40:("add","sp, sp, #8"),
        44:("pop","{r4, pc}"),
    }
    stage_zero={
        48:("add","r0, r4, #8"),
        52:("bl","#0x27f870"),
        56:("mov","r3, #1"),
        60:("str","r3, [r4]"),
        64:("b","#0x27ef90"),
    }
    stage_one={
        68:("add","r0, r4, #8"),
        72:("bl","#0x27fc68"),
        76:("cmp","r0, #0"),
        80:("bne","#0x27efc8"),
        84:("add","sp, sp, #8"),
        88:("pop","{r4, lr}"),
        92:("b","#0x27eecc"),
    }
    verified_global=all(selected.get(k)==v for k,v in frame_global_literal.items())
    verified_selectors=all(selected.get(k)==v for k,v in stage_selectors.items())
    return {
        "frame_global_pc_relative_literal":verified_global,
        "state_comparison_and_default_return":verified_selectors,
        "state_zero_call_then_store_one":verified_selectors and all(
            selected.get(k)==v for k,v in stage_zero.items()),
        "state_one_tick_result_branch":verified_selectors and all(
            selected.get(k)==v for k,v in stage_one.items()),
    }


def proven_original_code_helpers(windows: dict) -> dict[str, bool]:
    """Accept specific original ABI/control-flow semantics only on exact ARM."""
    def matched(name: str, expected: dict[int, tuple[str,str]]) -> bool:
        entries=windows.get(name,{}).get("instructions",[])
        got={e["offset"]:(e["op"],e["args"]) for e in entries}
        return all(got.get(k)==v for k,v in expected.items())
    zero_return=matched("game_entry_direct_callee",{
        0:("mov","r0, #0"),
        4:("bx","lr"),
    })
    entry_wrapper=matched("original_game_entry_jni",{
        0:("push","{fp, lr}"),
        4:("add","fp, sp, #4"),
        8:("sub","sp, sp, #8"),
        12:("str","r0, [fp, #-8]"),
        16:("str","r1, [fp, #-0xc]"),
        20:("mov","r0, #0"),
        24:("mov","r1, #0"),
        28:("bl","#0x27e974"),
        32:("mov","r3, r0"),
        36:("mov","r0, r3"),
        40:("sub","sp, fp, #4"),
        44:("pop","{fp, pc}"),
    })
    zero_chain=matched("frame_stage_zero_routine",{
        0:("push","{r4, lr}"),
        4:("mov","r4, r0"),
        8:("bl","#0x27f790"),
        12:("mov","r0, r4"),
        16:("pop","{r4, lr}"),
        20:("b","#0x280520"),
    })
    stage_one=matched("frame_stage_one_routine",{
        0:("push","{r4, lr}"),
        4:("ldr","r3, [r0, #0xc]"),
        8:("mov","r0, r3"),
        12:("ldr","r3, [r3]"),
        16:("mov","lr, pc"),
        20:("ldr","pc, [r3]"),
        24:("subs","r0, r0, #1"),
        28:("movne","r0, #1"),
        32:("pop","{r4, pc}"),
    })
    conditional_clock=matched("frame_stage_zero_second_callee",{
        24:("ldr","r1, [r4, #0x40]"),
        28:("ldr","r3, [r4, #0x44]"),
        32:("orrs","r1, r1, r3"),
        36:("bne","#0x28056c"),
    })
    gettimeofday_call=matched("frame_stage_zero_second_callee",{
        40:("add","r0, sp, #8"),
        44:("bl","#0x357c4"),
    })
    transition_state_two=matched("frame_state_one_zero_result_tail",{
        0:("push","{r4, r5, lr}"),
        4:("ldr","r4, [pc, #0x84]"),
        8:("mov","r3, #2"),
        12:("sub","sp, sp, #0x14"),
        16:("add","r4, pc, r4"),
        20:("str","r3, [r4], #8"),
    })
    nonzero_calls=matched("frame_state_one_nonzero_continuation",{
        0:("ldr","r1, [r4, #0x68]"),
        4:("mov","r3, #0"),
        8:("mov","r2, r3"),
        12:("ldr","ip, [r1]"),
        16:("mov","r0, r1"),
        20:("str","r3, [sp]"),
        24:("mov","r1, #0xe"),
        28:("mov","lr, pc"),
        32:("ldr","pc, [ip, #0x20]"),
        36:("ldr","r3, [r4, #0x68]"),
        40:("mov","r0, r3"),
        44:("ldr","r3, [r3]"),
        48:("mov","lr, pc"),
        52:("ldr","pc, [r3, #0x38]"),
        56:("b","#0x27ef90"),
    })
    signed_clock=matched("frame_stage_zero_second_callee",{
        48:("ldr","r2, [sp, #0xc]"),
        52:("ldr","r0, [sp, #8]"),
        56:("mov","r1, #0xf4000"),
        60:("add","r1, r1, #0x240"),
        64:("asr","r3, r2, #0x1f"),
        68:("smlal","r2, r3, r1, r0"),
        72:("strd","r2, r3, [r4, #0x40]"),
    })
    # Original ELF 0x281D54: allocator via original owner's virtual
    # slot 0x0C, conditional returned-record memory init, and global
    # pointer publication. The virtual allocator must NOT be fabricated.
    stage_zero_allocator_post_return=matched("stage_zero_followup_target_0x281d54",{
        0:("push","{r4, r5, lr}"),
        4:("ldr","r2, [pc, #0x64]"),
        8:("sub","sp, sp, #0xc"),
        12:("mov","r3, #4"),
        16:("mov","r4, #0"),
        20:("stm","sp, {r3, r4}"),
        24:("add","r2, pc, r2"),
        28:("ldr","ip, [r0]"),
        32:("mov","r1, #0x20"),
        36:("mov","r3, #1"),
        40:("mov","r5, r0"),
        44:("mov","lr, pc"),
        48:("ldr","pc, [ip, #0xc]"),
        52:("cmp","r0, #0"),
        56:("beq","#0x281db0"),
        60:("add","r3, r0, #4"),
        64:("str","r5, [r0, #0x18]"),
        68:("str","r4, [r0, #0x1c]"),
        72:("str","r3, [r0, #8]"),
        76:("str","r4, [r0, #0xc]"),
        80:("strb","r4, [r0, #0x10]"),
        84:("str","r4, [r0, #0x14]"),
        88:("str","r3, [r0, #4]"),
        92:("ldr","r3, [pc, #0x10]"),
        96:("add","r3, pc, r3"),
        100:("str","r0, [r3]"),
        104:("add","sp, sp, #0xc"),
        108:("pop","{r4, r5, pc}"),
    })
    # The original engine passes type 2 to 0x0028ca24. Prove the
    # dispatch table, selected case, float field read and function epilogue.
    stage_zero_type2=matched("stage_zero_setup_target_0x28ca24",{
        0:("vldr","s15, [pc, #0xf4]"),
        4:("push","{r4, r5, r6, lr}"),
        8:("sub","r3, r1, #1"),
        12:("vpush","{d8}"),
        16:("mov","r4, r0"),
        20:("vstr","s15, [r0, #0x14]"),
        24:("str","r1, [r0, #0x10]"),
        28:("cmp","r3, #5"),
        32:("addls","pc, pc, r3, lsl #2"),
        44:("b","#0x28cad8"),
        132:("vpop","{d8}"),
        136:("pop","{r4, r5, r6, pc}"),
    }) and matched("stage_zero_setup_type2_branch_0x28cad8",{
        0:("ldr","r3, [pc, #0x50]"),
        4:("add","r3, pc, r3"),
        8:("vldr","s15, [r3, #0x1c]"),
        12:("vstr","s15, [r0, #0x14]"),
        16:("b","#0x28caa8"),
    })
    # Entire original ELF 0x002833f4: three 32-bit stores and BX LR.
    # Its successor at +0x14 is a separate ARM function and is not decoded
    # as part of this one. All five instructions and the original caller
    # target must match before any native implementation is called verified.
    stage_zero_followup_stores=matched("stage_zero_followup_target_0x2833f4",{
        0:("mov","r3, #0"),
        4:("str","r3, [r0, #0x20]"),
        8:("str","r1, [r0, #0x1c]"),
        12:("str","r1, [r0, #0x24]"),
        16:("bx","lr"),
    })
    stage_zero_setup=matched("frame_stage_zero_second_callee",{
        0:("push","{r4, r5, lr}"),
        4:("mov","r1, #2"),
        8:("mov","r4, r0"),
        12:("sub","sp, sp, #0x14"),
        16:("add","r0, r0, #0x40"),
        20:("bl","#0x28ca24"),
        76:("ldr","r0, [r4, #4]"),
        80:("bl","#0x281d54"),
        84:("ldr","r1, [r4, #4]"),
        88:("add","r0, r4, #0x18"),
        92:("bl","#0x2833f4"),
    })
    return {
        "jni_run_entrypoint_always_returns_zero":zero_return and entry_wrapper,
        "stage_zero_calls_both_native_routines_in_order":zero_chain,
        "stage_one_normalizes_virtual_return_to_zero_or_one":stage_one,
        "original_signed_time_math_and_store_at_0x40":signed_clock,
        "original_clock_slot_zero_check_before_store":conditional_clock,
        "stage_zero_setup_and_two_followup_calls":stage_zero_setup,
        "stage_zero_followup_0x2833f4_complete_three_word_store":(
            stage_zero_setup and stage_zero_followup_stores),
        "stage_zero_type2_0x28ca24_complete_case":(
            stage_zero_setup and stage_zero_type2),
        "stage_zero_0x281d54_allocator_post_return_and_global_store":(
            stage_zero_setup and stage_zero_allocator_post_return),
        "original_gettimeofday_stack_arg_and_bl":conditional_clock and gettimeofday_call,
        "original_state_one_transition_stores_two_first":transition_state_two,
        "original_state_one_nonzero_two_vtable_calls":nonzero_calls,
    }


def observe(apk: Path) -> dict:
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
    except ImportError as exc:
        raise RuntimeError("Requires pinned Capstone on GitHub runner") from exc
    with ZipFile(apk) as archive:
        libs=[i for i in archive.infolist()
              if i.filename.startswith("lib/") and i.filename.endswith(".so")
              and 1024<i.file_size<=64*1024*1024 and not i.is_dir()]
        if len(libs)!=1:raise ValueError("Expected exactly one original ARM ELF")
        data=archive.read(libs[0])
        if len(data)!=libs[0].file_size:raise ValueError("Truncated ARM library")
    segs=elf32_load_ranges(data)
    sections=elf32_executable_sections(data)
    arm=Cs(CS_ARCH_ARM,CS_MODE_ARM)
    thumb=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
    out={}
    symbols={
        "on_draw_frame": "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame",
        "on_surface_changed": "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged",
        "on_surface_created": "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated",
        "game_entry": "Java_com_ea_DeadSpace_DeadSpace_runEntryPoint",
        "on_create": "Java_com_ea_blast_MainActivity_NativeOnCreate",
    }
    located=elf32_jni_symbols(data,set(symbols.values()))
    symbol_windows={}
    for short,name in symbols.items():
        sym=located.get(name)
        if sym is None:
            symbol_windows[short]={"present":False}
            continue
        addr,size,is_thumb=sym
        count=min(size,64)
        block=bounded_virtual_read(data,sections,addr,count)
        decoder=thumb if is_thumb else arm
        ins=list(decoder.disasm(block,addr))
        symbol_windows[short]={
            "present":True,
            "address":f"0x{addr:08x}",
            "symbol_size":size,
            "thumb":is_thumb,
            "instructions":[{"offset":i.address-addr,"op":i.mnemonic.lower(),
                             "args":i.op_str.lower()} for i in ins[:14]],
        }
    for key,(address,nbytes) in WINDOWS.items():
        evidence=mapping_comparison(data,sections,segs,address,nbytes)
        # The executable section table is the mapping used by the existing
        # symbol-level JNI verifier and independently by Ghidra. A PT_LOAD
        # mismatch must be surfaced, never silently used for translation.
        block=bounded_virtual_read(data,sections,address,nbytes)
        # The selected original library code paths are ARM instructions;
        # if a function uses Thumb, the address must come from an
        # independently verified Thumb symbol. Never infer instruction mode.
        decoded=list(arm.disasm(block,address))
        is_contiguous=(sum(i.size for i in decoded)==nbytes)
        out[key]={
            "address": f"0x{address:08x}",
            "file_backed": True,
            "mapping_evidence": evidence,
            "complete_arm_decode": is_contiguous,
            "instructions":[
                {"offset": i.address-address,
                 "op": i.mnemonic.lower(),
                 "args": i.op_str.lower()}
                for i in decoded[:40]],
        }
    # Two independently instruction-proven PC-relative literal loads.
    # Frame 0x27ef6c LDR r4,[pc,#0x90], 0x27ef74 ADD r4,pc,r4.
    # Transition 0x27eed0 LDR r4,[pc,#0x84], 0x27eedc ADD r4,pc,r4.
    frame_global=resolve_arm_literal_based_global(
        data,sections,0x0027ef6c,0x90,0x0027ef74)
    transition_global=resolve_arm_literal_based_global(
        data,sections,0x0027eed0,0x84,0x0027eedc)
    shared_original_global={
        "frame_state_global":f"0x{frame_global:08x}",
        "transition_state_global":f"0x{transition_global:08x}",
        "identical":frame_global==transition_global
    }
    if frame_global!=transition_global:
        raise ValueError("Original frame and state-two transition globals differ")
    # Original runtime global slot used after either allocator result:
    # LDR r3,[PC,#0x10] at 0x281DB0 and ADD r3,PC,r3 at 0x281DB4.
    original_record_slot=resolve_arm_literal_based_global(
        data,sections,0x00281db0,0x10,0x00281db4)
    type2_proof=proven_original_code_helpers(out).get(
        "stage_zero_type2_0x28ca24_complete_case")
    if not type2_proof:
        raise ValueError("Original ARM type-2 setup instruction proof changed")
    # ARM VLDR s15,[PC,#0xF4] at ELF 0x28CA24 reads literal at
    # 0x28CA24+8+0xF4 = 0x28CB20. This is a numeric float literal, not
    # any exported proprietary asset or decompressed game resource.
    initial_float_bits=struct.unpack("<I",bounded_virtual_read(
        data,sections,0x0028cb20,4))[0]
    type2_source_base=resolve_arm_literal_based_global(
        data,sections,0x0028cad8,0x50,0x0028cadc)
    original_global_layout=prove_runtime_global_layout(
        frame_global,original_record_slot,(type2_source_base+0x1c)&0xffffffff)
    type2_info={
        "entry_default_float_bits":f"0x{initial_float_bits:08x}",
        "original_runtime_field_address":f"0x{(type2_source_base+0x1c)&0xffffffff:08x}",
        "selected_case":2,
        "verified":True
    }
    contract=proven_frame_dispatch_prefix(
        out["native_frame_state_dispatch"]["instructions"])
    if not all(contract.values()):
        raise ValueError("Original ARM frame dispatch instruction contract changed")
    helpers=proven_original_code_helpers(out)
    if not all(helpers.values()):
        raise ValueError("Original ARM native helper instruction contract changed")
    return {
        "schema": 1,
        "kind": "original-ARM-targeted-instruction-evidence",
        "frame_dispatch_instruction_proof": contract,
        "frame_transition_global_address_proof":shared_original_global,
        "stage_zero_type2_runtime_float_evidence":type2_info,
        "stage_zero_0x281d54_record_global_slot":f"0x{original_record_slot:08x}",
        "verified_original_global_relative_layout":original_global_layout,
        "reconstructed_native_helper_proof": helpers,
        "windows": out,
        "original_elf_jni_symbols":symbol_windows,
        "limitations": (
          "Short original instruction windows only, not decompiled code. "
          "Direct JNI -> frame target may land inside a shared runtime routine. "
          "Do not call it a game update loop until side effects and state are "
          "proven. No original binary or gameplay assets published."
        ),
    }


def main() -> int:
    p=argparse.ArgumentParser()
    p.add_argument("--apk",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    report=observe(args.apk)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    print("Instruction windows recovered from real ARM library; no original bytes written.")
    print(json.dumps(report,sort_keys=True))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
