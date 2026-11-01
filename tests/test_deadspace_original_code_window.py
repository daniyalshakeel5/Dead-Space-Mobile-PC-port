"""Synthetic tests for bounded ELF32 ARM memory mapping; no real game bytes."""
import struct
import unittest
from tools.deadspace_original_code_window import (
    elf32_load_ranges, bounded_virtual_read, resolve_arm_literal_based_global,
    proven_frame_dispatch_prefix,
    proven_original_code_helpers)


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
                72:("strd","r2, r3, [r4, #0x40]")}),
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
        self.assertTrue(all(proven_original_code_helpers(windows).values()))
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

    def test_exact_original_arm_frame_dispatch_proof(self):
        specs={
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
