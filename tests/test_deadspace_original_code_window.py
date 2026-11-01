"""Synthetic tests for bounded ELF32 ARM memory mapping; no real game bytes."""
import struct
import unittest
from tools.deadspace_original_code_window import (
    elf32_load_ranges, bounded_virtual_read, resolve_arm_literal_based_global,
    proven_frame_dispatch_prefix,
    proven_original_code_helpers,prove_runtime_global_layout,
    resolve_arm_got_slot_from_pc_index)


class ArmWindowTests(unittest.TestCase):
    def make_elf(self):
        data=bytearray(512)
        data[:6]=b"\x7fELF\x01\x01"
        struct.pack_into("<I",data,28,52)  # e_phoff
        struct.pack_into("<HH",data,42,32,1)
        struct.pack_into("<IIIIIIII",data,52,
                         1,128,0x1000,0,16,16,5,4096)
        data[128:144]=bytes(range(16))
        return bytes(data)

    def test_original_registry_pc_indexed_got_address(self):
        binary=bytearray(96)
        spans=[(0x1000,0x1060,0)]
        # LDR at 0x1000, #0x24 -> literal at 0x102c.
        # LDR [PC,r3] at 0x1008 has PC=0x1010.
        struct.pack_into("<i",binary,0x2c,0x38)
        self.assertEqual(resolve_arm_got_slot_from_pc_index(
            binary,spans,0x1000,0x24,0x1008),0x1048)
        struct.pack_into("<i",binary,0x2c,-0x10)
        self.assertEqual(resolve_arm_got_slot_from_pc_index(
            binary,spans,0x1000,0x24,0x1008),0x1000)
        with self.assertRaises(ValueError):
            resolve_arm_got_slot_from_pc_index(
                binary,spans,0x1000,0x250,0x1008)

    def test_global_layout_requires_all_original_elf_addresses(self):
        layout=prove_runtime_global_layout(
            0x0049caa8,0x0049cb94,0x0049cd1c)
        self.assertEqual(layout["record_slot_relative_offset"],0xec)
        self.assertEqual(layout["type2_float_relative_offset"],0x274)
        self.assertEqual(layout["minimum_consecutive_span"],0x278)
        for a,b,c in ((0x0049caa8,0x0049cb98,0x0049cd1c),
                      (0x0049caa8,0x0049cb94,0x0049cd18),
                      (0x0049caa4,0x0049cb94,0x0049cd1c)):
            with self.assertRaises(ValueError):
                prove_runtime_global_layout(a,b,c)

    def test_exact_original_native_helper_proofs(self):
        def decoded(spec):
            return {"instructions":[{"offset":offset,"op":op,"args":args}
                                    for offset,(op,args) in spec.items()]}
        windows={
            "game_entry_direct_callee":decoded({
                0:("mov","r0, #0"),4:("bx","lr")}),
            "original_game_entry_jni":decoded({
                0:("push","{fp, lr}"),4:("add","fp, sp, #4"),
                8:("sub","sp, sp, #8"),12:("str","r0, [fp, #-8]"),
                16:("str","r1, [fp, #-0xc]"),
                20:("mov","r0, #0"),24:("mov","r1, #0"),
                28:("bl","#0x27e974"),32:("mov","r3, r0"),
                36:("mov","r0, r3"),40:("sub","sp, fp, #4"),
                44:("pop","{fp, pc}")}),
            "frame_stage_zero_routine":decoded({
                0:("push","{r4, lr}"),4:("mov","r4, r0"),
                8:("bl","#0x27f790"),12:("mov","r0, r4"),
                16:("pop","{r4, lr}"),20:("b","#0x280520")}),
            "stage_zero_owner_provider_0x27e930":decoded({
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
                60:("ldm","sp!, {pc}")}),
            "stage_zero_followup_target_0x281d54":decoded({
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
                108:("pop","{r4, r5, pc}")}),
            "stage_zero_setup_target_0x28ca24":decoded({
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
                136:("pop","{r4, r5, r6, pc}")}),
            "stage_zero_setup_type2_branch_0x28cad8":decoded({
                0:("ldr","r3, [pc, #0x50]"),
                4:("add","r3, pc, r3"),
                8:("vldr","s15, [r3, #0x1c]"),
                12:("vstr","s15, [r0, #0x14]"),
                16:("b","#0x28caa8")}),
            "stage_zero_followup_target_0x2833f4":decoded({
                0:("mov","r3, #0"),
                4:("str","r3, [r0, #0x20]"),
                8:("str","r1, [r0, #0x1c]"),
                12:("str","r1, [r0, #0x24]"),
                16:("bx","lr")}),
            "frame_stage_zero_second_callee":decoded({
                24:("ldr","r1, [r4, #0x40]"),
                28:("ldr","r3, [r4, #0x44]"),
                32:("orrs","r1, r1, r3"),
                36:("bne","#0x28056c"),
                40:("add","r0, sp, #8"),
                44:("bl","#0x357c4"),
                48:("ldr","r2, [sp, #0xc]"),
                52:("ldr","r0, [sp, #8]"),
                56:("mov","r1, #0xf4000"),
                60:("add","r1, r1, #0x240"),
                64:("asr","r3, r2, #0x1f"),
                68:("smlal","r2, r3, r1, r0"),
                72:("strd","r2, r3, [r4, #0x40]"),
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
                92:("bl","#0x2833f4")}),
            "frame_state_one_nonzero_continuation":decoded({
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
                56:("b","#0x27ef90")}),
            "frame_state_one_zero_result_tail":decoded({
                0:("push","{r4, r5, lr}"),
                4:("ldr","r4, [pc, #0x84]"),
                8:("mov","r3, #2"),
                12:("sub","sp, sp, #0x14"),
                16:("add","r4, pc, r4"),
                20:("str","r3, [r4], #8")}),
            "frame_stage_one_routine":decoded({
                0:("push","{r4, lr}"),4:("ldr","r3, [r0, #0xc]"),
                8:("mov","r0, r3"),12:("ldr","r3, [r3]"),
                16:("mov","lr, pc"),20:("ldr","pc, [r3]"),
                24:("subs","r0, r0, #1"),28:("movne","r0, #1"),
                32:("pop","{r4, pc}")})
        }
        windows["record_allocator_virtual_call_0x281d54"]=(
            windows["stage_zero_followup_target_0x281d54"])
        self.assertTrue(all(proven_original_code_helpers(windows).values()))
        # Mutation-based ARM instruction proof: any offset, register or
        # return opcode change invalidates this complete recovered routine.
        result_key="stage_zero_followup_0x2833f4_complete_three_word_store"
        stores=windows["stage_zero_followup_target_0x2833f4"]["instructions"]
        for index, changed in ((1,"r3, [r0, #0x24]"),
                               (2,"r0, [r0, #0x1c]"),
                               (3,"r1, [r0, #0x20]"),
                               (4,"r0")):
            old=stores[index]["args"]
            stores[index]["args"]=changed
            self.assertFalse(proven_original_code_helpers(windows)[result_key])
            stores[index]["args"]=old
        self.assertTrue(proven_original_code_helpers(windows)[result_key])
        abi_key="stage_zero_0x281d54_virtual_allocator_call_abi"
        self.assertTrue(proven_original_code_helpers(windows)[abi_key])
        allocator_abi=windows["record_allocator_virtual_call_0x281d54"]["instructions"]
        for off,bad in ((20,"sp, {r3}"),
                        (32,"r1, #0x24"),
                        (36,"r3, #0"),
                        (48,"pc, [ip, #0x10]")):
            obj=next(v for v in allocator_abi if v["offset"]==off)
            before=obj["args"]
            obj["args"]=bad
            self.assertFalse(proven_original_code_helpers(windows)[abi_key])
            obj["args"]=before
        query_key="stage_zero_0x27e930_original_registry_interface_query"
        self.assertTrue(proven_original_code_helpers(windows)[query_key])
        query=windows["stage_zero_owner_provider_0x27e930"]["instructions"]
        for off,bad in ((28,"r2, r2, #8"),
                        (48,"pc, [r3, #0x1c]"),
                        (52,"r0, [sp, #8]")):
            instruction=next(x for x in query if x["offset"]==off)
            original=instruction["args"]
            instruction["args"]=bad
            self.assertFalse(proven_original_code_helpers(windows)[query_key])
            instruction["args"]=original
        post_key="stage_zero_0x281d54_allocator_post_return_and_global_store"
        self.assertTrue(proven_original_code_helpers(windows)[post_key])
        allocator=windows["stage_zero_followup_target_0x281d54"]["instructions"]
        for off,bad in ((48,"pc, [ip, #0x10]"),
                        (56,"#0x281db4"),
                        (64,"r5, [r0, #0x14]"),
                        (88,"r3, [r0, #8]"),
                        (100,"r0, [r3, #4]")):
            obj=next(v for v in allocator if v["offset"]==off)
            old=obj["args"]
            obj["args"]=bad
            self.assertFalse(proven_original_code_helpers(windows)[post_key])
            obj["args"]=old
        name="stage_zero_type2_0x28ca24_complete_case"
        self.assertTrue(proven_original_code_helpers(windows)[name])
        type2=windows["stage_zero_setup_type2_branch_0x28cad8"]["instructions"]
        old=type2[2]["args"]
        type2[2]["args"]="s15, [r3, #0x18]"
        self.assertFalse(proven_original_code_helpers(windows)[name])
        type2[2]["args"]=old
        type2entry=windows["stage_zero_setup_target_0x28ca24"]["instructions"]
        selected=next(x for x in type2entry if x["offset"]==44)
        selected["args"]="#0x28caec"
        self.assertFalse(proven_original_code_helpers(windows)[name])
        selected["args"]="#0x28cad8"
        zero_steps=windows["frame_stage_zero_second_callee"]["instructions"]
        match=lambda off: next(item for item in zero_steps if item["offset"]==off)
        match(84)["args"]="r1, [r4, #8]"
        self.assertFalse(proven_original_code_helpers(windows)[
            "stage_zero_setup_and_two_followup_calls"])
        match(84)["args"]="r1, [r4, #4]"
        self.assertTrue(proven_original_code_helpers(windows)[
            "stage_zero_setup_and_two_followup_calls"])
        windows["game_entry_direct_callee"]["instructions"][0]["args"]="r0, #1"
        results=proven_original_code_helpers(windows)
        self.assertFalse(results["jni_run_entrypoint_always_returns_zero"])
        self.assertTrue(results["stage_zero_calls_both_native_routines_in_order"])
        windows["game_entry_direct_callee"]["instructions"][0]["args"]="r0, #0"
        windows["frame_stage_one_routine"]["instructions"][7]["op"]="mov"
        self.assertFalse(proven_original_code_helpers(windows)[
            "stage_one_normalizes_virtual_return_to_zero_or_one"])
        windows["frame_stage_one_routine"]["instructions"][7]["op"]="movne"
        windows["frame_stage_zero_second_callee"]["instructions"][7]["args"]="r1, r1, #0x244"
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_signed_time_math_and_store_at_0x40"])
        windows["frame_stage_zero_second_callee"]["instructions"][3]["args"]="#0x280570"
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_clock_slot_zero_check_before_store"])
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_gettimeofday_stack_arg_and_bl"])
        windows["frame_stage_zero_second_callee"]["instructions"][3]["args"]="#0x28056c"
        windows["frame_stage_zero_second_callee"]["instructions"][5]["args"]="#0x357c8"
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_gettimeofday_stack_arg_and_bl"])
        windows["frame_state_one_zero_result_tail"]["instructions"][5]["args"] = "r3, [r4, #4]"
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_state_one_transition_stores_two_first"])
        windows["frame_state_one_nonzero_continuation"]["instructions"][13]["args"] = "pc, [r3, #0x34]"
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_state_one_nonzero_two_vtable_calls"])
        windows["frame_state_one_nonzero_continuation"]["instructions"][13]["args"] = "pc, [r3, #0x38]"
        windows["frame_state_one_nonzero_continuation"]["instructions"][6]["args"] = "r1, #0xd"
        self.assertFalse(proven_original_code_helpers(windows)[
            "original_state_one_nonzero_two_vtable_calls"])

    def test_exact_original_arm_frame_dispatch_proof(self):
        specs={
            4:("ldr","r4, [pc, #0x90]"),
            12:("add","r4, pc, r4"),
            16:("ldr","r3, [r4]"),20:("cmp","r3, #0"),
            24:("beq","#0x27ef98"),28:("ldr","r3, [r4]"),
            32:("cmp","r3, #1"),36:("beq","#0x27efac"),
            40:("add","sp, sp, #8"),44:("pop","{r4, pc}"),
            48:("add","r0, r4, #8"),52:("bl","#0x27f870"),
            56:("mov","r3, #1"),60:("str","r3, [r4]"),
            64:("b","#0x27ef90"),68:("add","r0, r4, #8"),
            72:("bl","#0x27fc68"),76:("cmp","r0, #0"),
            80:("bne","#0x27efc8"),84:("add","sp, sp, #8"),
            88:("pop","{r4, lr}"),92:("b","#0x27eecc"),
        }
        code=[{"offset":at,"op":op,"args":args}
              for at,(op,args) in specs.items()]
        self.assertTrue(all(proven_frame_dispatch_prefix(code).values()))
        wrong_global=[dict(entry) for entry in code]
        next(x for x in wrong_global if x["offset"]==4)["args"]="r4, [pc, #0x94]"
        self.assertFalse(proven_frame_dispatch_prefix(wrong_global)[
            "frame_global_pc_relative_literal"])
        altered=[dict(entry) for entry in code]
        next(x for x in altered if x["offset"]==60)["args"]="r3, [r4, #4]"
        result=proven_frame_dispatch_prefix(altered)
        self.assertFalse(result["state_zero_call_then_store_one"])
        self.assertTrue(result["state_one_tick_result_branch"])
        altered=[dict(entry) for entry in code]
        next(x for x in altered if x["offset"]==80)["args"]="#0x27efbc"
        result=proven_frame_dispatch_prefix(altered)
        self.assertTrue(result["state_zero_call_then_store_one"])
        self.assertFalse(result["state_one_tick_result_branch"])

    def test_arm_pc_relative_signed_global_address(self):
        # Artificial ELF section and literal pool; never ship game bytes.
        contents=bytearray(64)
        spans=[(0x1000,0x1040,0)]
        struct.pack_into("<i",contents,0x0c,0x50)
        self.assertEqual(resolve_arm_literal_based_global(
            contents,spans,0x1000,4,0x1010),0x1068)
        struct.pack_into("<i",contents,0x0c,-0x20)
        self.assertEqual(resolve_arm_literal_based_global(
            contents,spans,0x1000,4,0x1010),0x0ff8)
        with self.assertRaises(ValueError):
            resolve_arm_literal_based_global(
                contents,spans,0x1000,0x500,0x1010)

    def test_file_backed_segment_and_exact_offsets(self):
        buf=self.make_elf()
        spans=elf32_load_ranges(buf)
        self.assertEqual(spans,[(0x1000,0x1010,128)])
        self.assertEqual(bounded_virtual_read(buf,spans,0x1004,4),
                         bytes([4,5,6,7]))
        self.assertEqual(bounded_virtual_read(buf,spans,0x100f,1),
                         bytes([15]))

    def test_rejects_bad_address_and_truncated_code(self):
        buf=self.make_elf()
        spans=elf32_load_ranges(buf)
        with self.assertRaises(ValueError):
            bounded_virtual_read(buf,spans,0x100f,2)
        with self.assertRaises(ValueError):
            bounded_virtual_read(buf,spans,0x1010,1)
        with self.assertRaises(ValueError):
            bounded_virtual_read(buf,spans,0x1000,257)

    def test_reject_elf_overflow(self):
        bad=bytearray(self.make_elf())
        struct.pack_into("<I",bad,52+16,400)
        with self.assertRaises(ValueError):
            elf32_load_ranges(bytes(bad))


if __name__=="__main__":
    unittest.main()
