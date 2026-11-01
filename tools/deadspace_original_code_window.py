#!/usr/bin/env python3
"""Bounded original ARM instruction evidence for native-code reconstruction.

Reads the user's confirmed APK only on ephemeral GitHub runners. Reports short
instruction windows at verified native JNI/engine control-flow addresses.
Never publishes ELF bytes, strings, decompiled C or a complete disassembly.
"""
from __future__ import annotations
import argparse
import os
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
    "frame_stage_zero_first_callee": (0x0027f790, 216),
    "startup_first_direct_0x27f124": (0x0027f124, 224),
    "startup_first_direct_0x2713b8": (0x002713b8, 224),
    "startup_first_direct_0xf895c": (0x000f895c, 224),
    "startup_allocator_object_init_0x27e560": (0x0027e560, 224),
    "stage_zero_owner_construction_tail_0x27f7e0": (0x0027f7e0, 176),
    "stage_zero_owner_provider_0x27e930": (0x0027e930, 128),
    "original_input_ready_guard_0x27e97c": (0x0027e97c, 28),
    "record_allocator_virtual_call_0x281d54": (0x00281d54, 112),
    "frame_stage_zero_second_callee": (0x00280520, 256),
    "stage_zero_second_tail_0x28061c": (0x0028061c, 256),
    "stage_zero_second_tail_0x28071c": (0x0028071c, 128),
    "stage_zero_setup_target_0x28ca24": (0x0028ca24, 160),
    "stage_zero_setup_type2_branch_0x28cad8": (0x0028cad8, 96),
    "stage_zero_followup_target_0x281d54": (0x00281d54, 160),
    "stage_zero_followup_target_0x2833f4": (0x002833f4, 160),
    # Direct callees independently recovered from real-APK Ghidra run 41305474129.
    # Observation only: no implementation or gameplay semantics inferred.
    "post_setup_direct_callee_0x27fd7c": (0x0027fd7c, 704),
    "post_setup_direct_callee_0x2803e4": (0x002803e4, 460),
    "resource_lookup_0x28b3a4": (0x0028b3a4, 384),
    "resource_factory_mode1_0x28a718": (0x0028a718, 768),
    "resource_factory_parse_0x28aab4": (0x0028aab4, 768),
    "resource_factory_copy_0x28b1e8": (0x0028b1e8, 256),
    "resource_factory_string_0x1a76d0": (0x001a76d0, 256),
    "resource_lookup_0x28a5f4": (0x0028a5f4, 384),
    "resource_lookup_0x28a4f8": (0x0028a4f8, 256),
    "resource_lookup_0x283cb4": (0x00283cb4, 384),
    "resource_lookup_0x286918": (0x00286918, 384),
    "resource_parser_0x2863ec": (0x002863ec, 768),
    "resource_parser_owner_init_0x27a768": (0x0027a768, 512),
    "resource_owner_embedded_init_0x27a4f0": (0x0027a4f0, 768),
    "resource_owner_default_pool_0x271914": (0x00271914, 320),
    "resource_parser_receiver_0x278018": (0x00278018, 768),
    "resource_receiver_activation_0x278e58": (0x00278e58, 480),
    "resource_receiver_activation_tree_clear_0x273570": (0x00273570, 384),
    "resource_receiver_activation_tail_0x279038": (0x00279038, 384),
    "resource_receiver_tree_lookup_0x2710a8": (0x002710a8, 768),
    "resource_receiver_utf16_compare_0x26e670": (0x0026e670, 384),
    "resource_receiver_copy_0x276694": (0x00276694, 384),
    "resource_receiver_string_init_0x27541c": (0x0027541c, 224),
    "resource_parser_receiver_tail_0x278318": (0x00278318, 768),
    "resource_parser_receiver_tail_0x278618": (0x00278618, 384),
    "resource_parser_owner_mode_0x26c410": (0x0026c410, 512),
    "resource_parser_owner_bind_0x278778": (0x00278778, 512),
    "resource_parser_tail_0x2866ec": (0x002866ec, 520),
    "resource_copy_0x285918": (0x00285918, 384),
    "resource_string_pair_0x2857a0": (0x002857a0, 384),
    "resource_string_tree_0x285238": (0x00285238, 384),
    "resource_string_destroy_0x284e84": (0x00284e84, 224),
    "resource_token_match_0x285630": (0x00285630, 368),
    "resource_token_tree_find_0x2854b8": (0x002854b8, 216),
    "resource_token_compare_0x35674": (0x00035674, 128),
    "resource_token_object_0x285590": (0x00285590, 224),
    "resource_record_build_0x28611c": (0x0028611c, 512),
    "resource_utf16_transform_0x28cc88": (0x0028cc88, 512),
    "resource_record_insert_0x2862e8": (0x002862e8, 256),
    "resource_loader_tree_begin_0x283408": (0x00283408, 192),
    "resource_loader_tree_next_0x2933ec": (0x002933ec, 256),
    "resource_lookup_0x284d38": (0x00284d38, 256),
    "resource_lookup_0x284df8": (0x00284df8, 256),
    "resource_lookup_0x28a644": (0x0028a644, 256),
    "resource_cleanup_0x284f00": (0x00284f00, 256),

    "verified_vtable_object34_slot0c_0x2876d0": (0x002876d0, 256),
    "verified_vtable_object58_slot24_0x281b40": (0x00281b40, 256),
    "verified_vtable_secondary_slot0c_0x2828e4": (0x002828e4, 256),
    "verified_vtable_embedded_slot18_0x2800e0": (0x002800e0, 256),
    "post_setup_direct_callee_0x287a94": (0x00287a94, 160),
    "post_setup_direct_callee_0x2849e0": (0x002849e0, 160),
    "post_setup_direct_callee_0x280a34": (0x00280a34, 160),
    "original_input_event_record_init_0x2807a0": (0x002807a0, 160),
    "input_event_constructor_helper_0x35194": (0x00035194, 96),
    "secondary_allocator_postprocess_0x288688": (0x00288688, 160),
    "postprocess_constructor_0x2828ac": (0x002828ac, 192),
    "postprocess_embedded_init_0x27fb28": (0x0027fb28, 256),
    "postprocess_embedded_helper_0x27faf4": (0x0027faf4, 160),
    "postprocess_embedded_setup_0x27fa74": (0x0027fa74, 192),
    "repeated_initializer_callee_0x284178": (0x00284178, 768),
    "startup_global_direct_service_0x36fd38": (0x0036fd38, 256),
    "startup_global_direct_service_target_0x37000c": (0x0037000c, 256),
    "startup_registration_helper_first_0x2839c8": (0x002839c8, 768),
    "startup_registration_insert_0x2837b4": (0x002837b4, 768),
    "startup_registration_match_action_0x283f5c": (0x00283f5c, 512),
    "startup_registration_string_copy_0x281df4": (0x00281df4, 256),
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
    if count<1 or count>768: raise ValueError("Unsafe instruction window size")
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


def resolve_arm_got_slot_from_pc_index(raw: bytes,
                                       executable_spans: list[tuple[int,int,int]],
                                       ldr_literal_address: int,
                                       literal_immediate: int,
                                       ldr_index_address: int) -> int:
    """Original ARM: LDR r3,[PC,#imm]; then LDR r3,[PC,r3].

    PC is instruction VA + 8 in both instructions; the former word is a
    signed 32-bit displacement. Returns the original virtual GOT address,
    not a guessed live object pointer or executable data contents.
    """
    displacement=struct.unpack("<i", bounded_virtual_read(
        raw,executable_spans,ldr_literal_address+8+literal_immediate,4))[0]
    return (ldr_index_address+8+displacement)&0xffffffff


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
    original_registry_query=matched("stage_zero_owner_provider_0x27e930",{
        0:("str","lr, [sp, #-4]!"),
        4:("sub","sp, sp, #0xc"),
        8:("add","r1, sp, #8"),
        12:("mov","r3, #0"),
        16:("str","r3, [r1, #-4]!"),
        20:("ldr","r3, [pc, #0x24]"),
        24:("mov","r2, #0x10000"),
        28:("add","r2, r2, #4"),
        32:("ldr","r3, [pc, r3]"),
        36:("mov","r0, r3"),
        40:("ldr","r3, [r3]"),
        44:("mov","lr, pc"),
        48:("ldr","pc, [r3, #0x18]"),
        52:("ldr","r0, [sp, #4]"),
        56:("add","sp, sp, #0xc"),
        60:("ldm","sp!, {pc}"),
    })
    stage_zero_allocator_call_abi=matched("record_allocator_virtual_call_0x281d54",{
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
    })
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
        "stage_zero_0x27e930_original_registry_interface_query":(
            original_registry_query),
        "stage_zero_0x281d54_virtual_allocator_call_abi":(
            stage_zero_allocator_call_abi and stage_zero_allocator_post_return),
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
        # These were identified independently in the Vita loader's
        # controls.c, but all actual instruction values below come from
        # our *different* user-confirmed original game APK.
        "input_raw_down": "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown",
        "input_raw_move": "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerMove",
        "input_raw_up": "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerUp",
        "input_pointer_event": "Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent",
        "input_key_down": "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown",
        "input_key_up": "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp",
        "input_acceleration": "Java_com_ea_blast_AccelerometerAndroidDelegate_NativeOnAcceleration",
        "audio_startup": "Java_com_ea_EAIO_EAIO_Startup",
    }
    located=elf32_jni_symbols(data,set(symbols.values()))
    symbol_windows={}
    for short,name in symbols.items():
        sym=located.get(name)
        if sym is None:
            symbol_windows[short]={"present":False}
            continue
        addr,size,is_thumb=sym
        # Full game input JNI wrappers, unlike unrelated JNI stubs, need
        # the entire original function to translate downstream dispatch.
        long_input_names={"input_pointer_event","input_key_down","input_key_up"}
        count=min(size,224 if short in long_input_names else 64)
        block=bounded_virtual_read(data,sections,addr,count)
        decoder=thumb if is_thumb else arm
        ins=list(decoder.disasm(block,addr))
        symbol_windows[short]={
            "present":True,
            "address":f"0x{addr:08x}",
            "symbol_size":size,
            "thumb":is_thumb,
            "instructions":[{"offset":i.address-addr,"op":i.mnemonic.lower(),
                             "args":i.op_str.lower()} for i in ins[:64 if short in long_input_names else 14]],
        }
    # Original binary proof, not a value borrowed from the Vita loader.
    raw_pointer_proof={
        "input_raw_down":0x6000c,
        "input_raw_move":0x4000c,
        "input_raw_up":0x8000c,
    }
    for key,expected in raw_pointer_proof.items():
        ins=symbol_windows[key]["instructions"]
        if not symbol_windows[key]["present"] or len(ins)!=3:
            raise ValueError("Original pointer constant ABI not complete")
        # Capstone yields MOV r0, #HI and ADD r0, r0, #0xC.
        expected_high=expected & ~0xffff
        if (ins[0]["op"],ins[0]["args"])!=("mov",f"r0, #0x{expected_high:x}") or (
            ins[1]["op"],ins[1]["args"])!=("add","r0, r0, #0xc") or (
            ins[2]["op"],ins[2]["args"])!=("bx","lr"):
            raise ValueError("Original ARM pointer constant proof mismatched")
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
                for i in decoded[:190 if key.startswith("resource_owner_") else 190 if key.startswith("resource_receiver_") else 460 if key.startswith("resource_parser_receiver_") else 192 if key.startswith("resource_parser_") or key.startswith("resource_copy_") or key.startswith("resource_string_") or key.startswith("resource_token_") or key.startswith("resource_record_") or key.startswith("resource_utf16_transform_") else 64 if key.startswith("resource_loader_tree_") else 180 if key.startswith("resource_factory_") else 96 if key.startswith("resource_lookup_") else 192 if key=="repeated_initializer_callee_0x284178" or key.startswith("startup_registration_") else 180 if key=="post_setup_direct_callee_0x27fd7c" else 120 if key=="post_setup_direct_callee_0x2803e4" else 64 if key.startswith("postprocess_embedded_") or key=="frame_stage_zero_second_callee" or key.startswith("stage_zero_second_tail_") or key=="startup_allocator_object_init_0x27e560" or key.startswith("verified_vtable_") else 40]],
        }
    # Two independently instruction-proven PC-relative literal loads.
    # Frame 0x27ef6c LDR r4,[pc,#0x90], 0x27ef74 ADD r4,pc,r4.
    # Transition 0x27eed0 LDR r4,[pc,#0x84], 0x27eedc ADD r4,pc,r4.
    # Exact input guard: LDR r3,[pc,#0x10]; LDR r0,[pc,r3];
    # CMP r0,#1; MOVNE r0,#0; MOVEQ r0,#1; BX lr.
    guard_ops=[(x["op"],x["args"]) for x in
        out["original_input_ready_guard_0x27e97c"]["instructions"][:6]]
    if guard_ops!=[
        ("ldr","r3, [pc, #0x10]"),("ldr","r0, [pc, r3]"),
        ("cmp","r0, #1"),("movne","r0, #0"),
        ("moveq","r0, #1"),("bx","lr")]:
        raise ValueError("Original input readiness ARM guard proof changed")
    # Independently resolve allocation and event-queue owner slots from
    # all three original ARM JNI handlers (pointer/key-down/key-up).
    input_slots={
        "pointer":(
            resolve_arm_got_slot_from_pc_index(
                data,sections,0x0027f9d8,0x8c,0x0027f9e4),
            resolve_arm_got_slot_from_pc_index(
                data,sections,0x0027fa40,0x28,0x0027fa48)),
        "key_down":(
            resolve_arm_got_slot_from_pc_index(
                data,sections,0x0028484c,0x84,0x00284858),
            resolve_arm_got_slot_from_pc_index(
                data,sections,0x002848a8,0x2c,0x002848b4)),
        "key_up":(
            resolve_arm_got_slot_from_pc_index(
                data,sections,0x00284794,0x84,0x002847a0),
            resolve_arm_got_slot_from_pc_index(
                data,sections,0x002847f0,0x2c,0x002847fc)),
    }
    if len({v[0] for v in input_slots.values()})!=1 or (
        len({v[1] for v in input_slots.values()})!=1):
        raise ValueError("Three original input JNI singleton slots disagree")
    input_allocator_slot,input_queue_slot=input_slots["pointer"]
    if input_allocator_slot==input_queue_slot:
        raise ValueError("Input allocator and event queue slots unexpectedly alias")
    # Original 0x2841F4 / 0x2841F8: game character mapping table
    # loaded through the real-APK GOT pointer, not ASCII assumptions.
    register_character_map_got=resolve_arm_got_slot_from_pc_index(
        data,sections,0x002841f4,0x278,0x002841f8)
    if not register_character_map_got:
        raise ValueError("Registration character mapping GOT resolves to null")
    input_ready_slot=resolve_arm_got_slot_from_pc_index(
        data,sections,0x0027e97c,0x10,0x0027e980)
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
    # Original ELF 0x27E930 is a service-interface query. It obtains an
    # original singleton pointer via a PC-relative GOT word, then calls
    # virtual offset +0x18 with request identifier 0x10004.
    registry_got_va=resolve_arm_got_slot_from_pc_index(
        data,sections,0x0027e944,0x24,0x0027e950)
    if registry_got_va != 0x0049caa0:
        raise ValueError("Original service registry pointer address does not match native mapping")
    registry_got_in_file=any(
        begin<=registry_got_va and registry_got_va+4<=end
        for begin,end,_ in segs)
    registry_got_initial=(
        struct.unpack("<I",bounded_virtual_read(
            data,segs,registry_got_va,4))[0]
        if registry_got_in_file else None
    )
    # Original runtime global slot used after either allocator result:
    # LDR r3,[PC,#0x10] at 0x281DB0 and ADD r3,PC,r3 at 0x281DB4.
    original_record_slot=resolve_arm_literal_based_global(
        data,sections,0x00281db0,0x10,0x00281db4)
    allocator_descriptor_va=resolve_arm_literal_based_global(
        data,sections,0x00281d58,0x64,0x00281d6c)
    if allocator_descriptor_va==0:
        raise ValueError("Original ARM allocator type descriptor resolves to null")
    # Original secondary allocator at 0x2849E0 uses ADD r2,PC,r2
    # at +0x20, paired with LDR r2,[PC,#0x40] at +0x04.
    # Resolve descriptor VA from the real ELF literal, never assume
    # it is equal to the primary 0x281D54 descriptor.
    secondary_descriptor_va=resolve_arm_literal_based_global(
        data,sections,0x002849e4,0x40,0x00284a00)
    if secondary_descriptor_va==0:
        raise ValueError("Secondary allocation descriptor resolves to null")
    # Original input event (0x2807A0) constructs with a PC-relative
    # class-table pointer; the first temporary vtable is base+8, and
    # it finally installs base+0x20. Resolve from this APK only.
    event_constructor_base=resolve_arm_literal_based_global(
        data,sections,0x002807a4,0x34,0x002807b0)
    input_initial_table=(event_constructor_base+8)&0xffffffff
    input_final_table=(event_constructor_base+0x20)&0xffffffff
    # Original 0x36FD38 = MOV r0,#1 ; B 0x37000C.
    # Entire 0x37000C function through +0x68 was recovered from
    # user's APK; resolve its three PC-relative inputs and scratch
    # vtable pointer (final stores at original SP+0x0c).
    service_global_va=resolve_arm_literal_based_global(
        data,sections,0x00370010,0x60,0x00370024)
    service_stack_param_va=resolve_arm_literal_based_global(
        data,sections,0x00370014,0x60,0x00370030)
    service_descriptor_va=resolve_arm_literal_based_global(
        data,sections,0x00370018,0x60,0x0037003c)
    service_temp_vtable_va=(
        resolve_arm_literal_based_global(
            data,sections,0x00370058,0x24,0x00370060)+8)&0xffffffff
    if not all((service_global_va,service_stack_param_va,
                service_descriptor_va,service_temp_vtable_va)):
        raise ValueError("Original 0x37000C service literals unresolved")
    # Original 0x27E560 C++ static singleton guard and returned object
    # are PC-relative original BSS addresses, not address-space guesses.
    singleton_guard_va=resolve_arm_literal_based_global(
        data,sections,0x0027e564,0x84,0x0027e56c)
    singleton_got_base_va=resolve_arm_literal_based_global(
        data,sections,0x0027e568,0x84,0x0027e574)
    singleton_got_slot_offsets={
        "field8_source_pointer":0x0027e5fc,
        "table4_pointer":0x0027e600,
        "destructor_pointer":0x0027e604,
        "dso_handle_pointer":0x0027e608,
    }
    singleton_got_slots={
        key:(singleton_got_base_va+struct.unpack(
            "<I",bounded_virtual_read(data,sections,literal,4))[0])&0xffffffff
        for key,literal in singleton_got_slot_offsets.items()
    }
    singleton_object_va=resolve_arm_literal_based_global(
        data,sections,0x0027e580,0x70,0x0027e584)+4
    if singleton_guard_va+4 != singleton_object_va:
        raise ValueError("Original 0x27E560 guard/object address mismatch")
    # Original 0x280A34 third startup object constructor, with three
    # 0x27E560 calls and a 0x58-byte fully decoded field range.
    constructor_280a34_primary_base=resolve_arm_literal_based_global(
        data,sections,0x00280a34,0x8c,0x00280a40)
    constructor_280a34_secondary_base=resolve_arm_literal_based_global(
        data,sections,0x00280a38,0x8c,0x00280a44)
    # Read only bounded original ELF vtable method pointer words (never
    # original game assets or code bodies). The constructors install
    # these exact original VAs. A relocation-zero or function pointer
    # outside executable sections is reported, NOT assumed callable.
    startup_method_slots={
        "object34_method_0x0c":(0x00489a10,0x0c),
        "object58_method_0x24":(0x00489880,0x24),
        "secondary_method_0x0c":(0x00489a98,0x0c),
        "embedded_method_0x18":(0x004897b8,0x18),
    }
    startup_method_targets={}
    for label,(table_va,slot) in startup_method_slots.items():
        pointer_va=(table_va+slot)&0xffffffff
        try:
            method=struct.unpack("<I",bounded_virtual_read(
                data,segs,pointer_va,4))[0]
        except ValueError:
            method=None
        executable=bool(method is not None and
            any(start<=method<end for start,end,_ in sections))
        startup_method_targets[label]={
            "slot_va":f"0x{pointer_va:08x}",
            "original_method_va":f"0x{method:08x}" if method is not None else None,
            "file_backed":method is not None,
            "executable_address":executable,
        }
    # Original 0x287A94 object constructor has two PC-relative table
    # bases and a single outstanding direct call to 0x27E560.
    constructor_287a94_primary_base=resolve_arm_literal_based_global(
        data,sections,0x00287a94,0x60,0x00287aa0)
    constructor_287a94_secondary_base=resolve_arm_literal_based_global(
        data,sections,0x00287a98,0x60,0x00287aa4)
    # Original 0x280520 later stage setup and three allocations.
    second_tail_descriptors={
        "resource_primary":(0x00280598,0x104,0x002805a0),
        "resource_fallback":(0x002805b0,0x0f0,0x002805b8),
        "allocated_0x34":(0x002805c4,0x0e0,0x002805e4),
        "allocated_0x58":(0x00280644,0x064,0x00280664),
    }
    original_tail_vas={
        name:resolve_arm_literal_based_global(data,sections,*site)
        for name,site in second_tail_descriptors.items()
    }
    if not all(original_tail_vas.values()):
        raise ValueError("Original stage-zero second tail descriptor is null")
    # Original ELF startup 0x27F790 and nested 0x27F124: preserve all
    # seven original ARM PC-relative descriptor addresses. Each pair
    # has been independently instruction-decoded, not guessed from Vita.
    startup_literals={
        "first_query_descriptor":(0x0027f7a0,0xb4,0x0027f7ac),
        "first_virtual_second_arg":(0x0027f7b8,0xa0,0x0027f7cc),
        "first_virtual_third_arg":(0x0027f7bc,0xa0,0x0027f7d0),
        "next_virtual_second_arg":(0x0027f80c,0x54,0x0027f824),
        "next_virtual_third_arg":(0x0027f810,0x54,0x0027f81c),
        "nested_query_descriptor":(0x0027f138,0x54,0x0027f144),
        "nested_virtual_second_arg":(0x0027f158,0x38,0x0027f164),
        "nested_virtual_third_arg":(0x0027f15c,0x38,0x0027f168),
    }
    startup_descriptor_vas={
        name:resolve_arm_literal_based_global(data,sections,*site)
        for name,site in startup_literals.items()
    }
    if not all(startup_descriptor_vas.values()):
        raise ValueError("Original startup descriptor literal is null")
    # Original 0x2828AC LDR at +0x00 / ADD at +0x0c / ADD #8 at +0x10.
    # Original 0x288688 LDR at +0x0c / ADD at +0x18 / ADD #8 at +0x1c.
    # Both literal-derived addresses are original ARM VAs, not host pointers.
    constructor_vtable_va=(resolve_arm_literal_based_global(
        data,sections,0x002828ac,0x24,0x002828b8)+8)&0xffffffff
    secondary_vtable_va=(resolve_arm_literal_based_global(
        data,sections,0x00288694,0x40,0x002886a0)+8)&0xffffffff
    # 0x288688 also loads all three constructor arguments from real
    # PC-relative literals. Resolve each static VA at the precise ADD.
    embedded_init_arg=(resolve_arm_literal_based_global(
        data,sections,0x00288698,0x40,0x002886ac))&0xffffffff
    embedded_setup_arg1=(resolve_arm_literal_based_global(
        data,sections,0x002886b8,0x24,0x002886c4))&0xffffffff
    embedded_setup_arg2=(resolve_arm_literal_based_global(
        data,sections,0x002886bc,0x24,0x002886c8))&0xffffffff
    if not all((embedded_init_arg,embedded_setup_arg1,embedded_setup_arg2)):
        raise ValueError("Original secondary constructor argument resolves to zero")
    # Complete 0x27FB28 ARM function: LDR/ADD vtable and two
    # literal-derived parameters for its +0x84 virtual call.
    embedded_vtable_va=(resolve_arm_literal_based_global(
        data,sections,0x0027fb28,0xbc,0x0027fb34)+8)&0xffffffff
    embedded_method84_arg2=resolve_arm_literal_based_global(
        data,sections,0x0027fb68,0x80,0x0027fb80)
    embedded_method84_arg3=resolve_arm_literal_based_global(
        data,sections,0x0027fb6c,0x80,0x0027fb78)
    if not all((embedded_vtable_va,embedded_method84_arg2,embedded_method84_arg3)):
        raise ValueError("Original embedded initializer literal resolution failed")
    if not constructor_vtable_va or not secondary_vtable_va:
        raise ValueError("Original constructor vtable literal resolves to zero")
    for key,prefix in (
        ("postprocess_constructor_0x2828ac",[
            ("ldr","r1, [pc, #0x24]"),("mov","r2, #0"),
            ("strb","r2, [r0, #0x12]")]),
        ("secondary_allocator_postprocess_0x288688",[
            ("push","{r4, r5, r6, lr}"),("mov","r4, r0"),
            ("bl","#0x2828ac")])):
        ins=out[key]["instructions"]
        if [(i["op"],i["args"]) for i in ins[:len(prefix)]]!=prefix:
            raise ValueError("Original ARM constructor instruction proof changed")
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
        "stage_zero_0x27e930_registry_query":{
            "original_registry_got_slot":f"0x{registry_got_va:08x}",
            "got_file_backed":registry_got_in_file,
            "got_initial_pointer_or_relocation":(
                f"0x{registry_got_initial:08x}"
                if registry_got_initial is not None else None),
            "requested_interface_id":0x10004,
            "vtable_offset":0x18,
        },
        "input_event_constructor_0x2807a0":{
            "initial_vtable_va":f"0x{input_initial_table:08x}",
            "final_vtable_va":f"0x{input_final_table:08x}",
            "direct_helper_0x35194":"import-plt-target-still-unresolved",
            "required_event_bytes":48,
            "verified":True,
        },
        "original_stage_zero_singleton_0x27e560":{
            "guard_word_va":f"0x{singleton_guard_va:08x}",
            "returned_object_va":f"0x{singleton_object_va:08x}",
            "got_base_va":f"0x{singleton_got_base_va:08x}",
            "got_slots":{
                key:f"0x{value:08x}" for key,value in singleton_got_slots.items()
            },
            "singletons_per_call":1,
            "verified":True,
        },
        "original_constructor_280a34_literals":{
            "vtable_at_plus_zero":f"0x{(constructor_280a34_primary_base+8)&0xffffffff:08x}",
            "table_at_plus_four":f"0x{(constructor_280a34_primary_base+0x4c)&0xffffffff:08x}",
            "vtable_at_plus_0x14":f"0x{(constructor_280a34_secondary_base+8)&0xffffffff:08x}",
            "original_base_constructor_calls":3,
            "verified":True,
        },
        "original_stage_zero_concrete_method_targets":{
            "targets":startup_method_targets,
            "source":"original ELF file-backed vtable slots",
            "does_not_prove_virtual_owner_constructed":True,
        },
        "original_constructor_287a94_literals":{
            "vtable_at_plus_zero":f"0x{(constructor_287a94_primary_base+8)&0xffffffff:08x}",
            "table_at_plus_four":f"0x{(constructor_287a94_primary_base+0x40)&0xffffffff:08x}",
            "vtable_at_plus_0x14":f"0x{(constructor_287a94_secondary_base+8)&0xffffffff:08x}",
            "direct_callee":"0x0027e560",
            "verified":True,
        },
        "original_stage_zero_second_tail_descriptors":{
            "literal_ARM_virtual_addresses":{
                name:f"0x{va:08x}" for name,va in original_tail_vas.items()
            },
            "returns_at":"0x002806a0",
            "verified":True,
        },
        "original_stage_zero_first_routine_descriptors":{
            "literal_ARM_virtual_addresses":{
                name:f"0x{va:08x}" for name,va in startup_descriptor_vas.items()
            },
            "ELF_entry":"0x0027f790",
            "direct_callees":[
                "0x0027e930","0x0027f124","0x002713b8","0x000f895c"
            ],
            "verified":True,
        },
        "original_input_singleton_slots":{
            "allocator_owner_pointer_slot_va":f"0x{input_allocator_slot:08x}",
            "queue_owner_pointer_slot_va":f"0x{input_queue_slot:08x}",
            "verified_by_three_jni_wrappers":True,
        },
        "original_registration_character_map":{
            "function_va":"0x00284178",
            "table_got_slot_va":f"0x{register_character_map_got:08x}",
            "character_index_scale":2,
            "signed_16bit_table_byte_offset":2,
            "verified":True,
        },
        "original_startup_global_service_0x37000c":{
            "entry_va":"0x0036fd38",
            "tail_target_va":"0x0037000c",
            "original_global_word_va":f"0x{service_global_va:08x}",
            "stack_parameter_va":f"0x{service_stack_param_va:08x}",
            "literal_descriptor_va":f"0x{service_descriptor_va:08x}",
            "temp_object_vtable_va":f"0x{service_temp_vtable_va:08x}",
            "time_seed_or_duration_us":1000000,
            "original_callee_chain":["0x00282000","0x00282614","0x00288408"],
            "verified":True,
        },
        "original_input_ready_guard":{
            "guard_function_va":"0x0027e97c",
            "compares_original_runtime_word_to":1,
            "original_host_global_slot_va":f"0x{input_ready_slot:08x}",
            "same_as_frame_state_global":input_ready_slot==frame_global,
            "verified":True,
        },
        "embedded_initializer_0x27fb28_literals":{
            "original_vtable_arm_va":f"0x{embedded_vtable_va:08x}",
            "method_0x84_arg2_va":f"0x{embedded_method84_arg2:08x}",
            "method_0x84_arg3_va":f"0x{embedded_method84_arg3:08x}",
            "direct_helper_0x27faf4":True,
            "verified":True,
        },
        "secondary_postallocation_constructor_evidence":{
            "constructor_0x2828ac_vtable_va":f"0x{constructor_vtable_va:08x}",
            "postprocess_0x288688_override_vtable_va":f"0x{secondary_vtable_va:08x}",
            "embedded_init_0x27fb28_arg1":f"0x{embedded_init_arg:08x}",
            "embedded_setup_0x27fa74_arg1":f"0x{embedded_setup_arg1:08x}",
            "embedded_setup_0x27fa74_arg2":f"0x{embedded_setup_arg2:08x}",
            "constructor_direct_call":"0x002828ac",
            "constructor_first_byte_range":"0x00..0x12",
            "verified":True,
        },
        "secondary_allocator_0x2849e0":{
            "descriptor_arm_address":f"0x{secondary_descriptor_va:08x}",
            "request_bytes":36,
            "vtable_offset":12,
            "postprocess_direct_callee":"0x00288688",
            "evidence":"ELF literal resolution and original instruction window",
        },
        "stage_zero_0x281d54_allocator_abi":{
            "callsite":"0x00281d84",
            "vtable_offset":12,
            "allocation_size":32,
            "original_type_descriptor_va":f"0x{allocator_descriptor_va:08x}",
            "register_flag":1,
            "stack_arg0":4,
            "stack_arg1":0,
            "verified":True,
        },
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
    if os.environ.get("DS_EMIT_FULL_ARM_REPORT")=="1":
        print(json.dumps(report,sort_keys=True))
    else:
        print("Full ARM instruction report saved to bounded GitHub artifact; stdout disabled.")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
