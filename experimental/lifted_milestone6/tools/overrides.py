"""Native C replacements for libgcc runtime helpers (division, soft-float conversions, compares).
They replace hand-written ARM assembly that jumps between helpers; the native versions are also much faster."""
D = "u32 lo, hi; "
def dret(expr): return D + f"d2u({expr}, &lo, &hi); return PACK(lo, hi);"
A, B = "u2d(r0, r1)", "u2d(r2, r3)"
OVERRIDES = {
    "__aeabi_ui2d":  dret("(double)r0"),
    "__floatsidf":   dret("(double)(s32)r0"),
    "__extendsfdf2": dret("(double)u2f(r0)"),
    "__aeabi_ul2d":  dret("(double)(u64)PACK(r0, r1)"),
    "__floatdidf":   dret("(double)(s64)PACK(r0, r1)"),
    "__aeabi_ui2f":  "return f2u((float)r0);",
    "__floatsisf":   "return f2u((float)(s32)r0);",
    "__aeabi_ddiv":  dret(f"{A} / {B}"),
    "__muldf3":      dret(f"{A} * {B}"),
    "__divsf3":      "return f2u(u2f(r0) / u2f(r1));",
    "__aeabi_fcmpeq": "return u2f(r0) == u2f(r1);",
    "__aeabi_fcmpge": "return u2f(r0) >= u2f(r1);",
    "__aeabi_fcmpgt": "return u2f(r0) >  u2f(r1);",
    "__aeabi_fcmple": "return u2f(r0) <= u2f(r1);",
    "__aeabi_fcmplt": "return u2f(r0) <  u2f(r1);",
    "__aeabi_dcmpeq": f"return {A} == {B};",
    "__aeabi_dcmpge": f"return {A} >= {B};",
    "__aeabi_dcmpgt": f"return {A} >  {B};",
    "__aeabi_dcmple": f"return {A} <= {B};",
    "__aeabi_dcmplt": f"return {A} <  {B};",
    "__udivsi3": "return r1 ? r0 / r1 : 0;",
    "__umodsi3": "return r1 ? r0 % r1 : 0;",
    "__divsi3":  "if (!r1) return 0; if ((s32)r0 == INT32_MIN && (s32)r1 == -1) return r0; return (u32)((s32)r0 / (s32)r1);",
    "__modsi3":  "if (!r1) return 0; if ((s32)r1 == -1) return 0; return (u32)((s32)r0 % (s32)r1);",
    "__absvsi2": 'if ((s32)r0 == INT32_MIN) trap_unsupported(0, "__absvsi2 overflow"); return (s32)r0 < 0 ? 0u - r0 : r0;',
    "__negvsi2": 'if ((s32)r0 == INT32_MIN) trap_unsupported(0, "__negvsi2 overflow"); return 0u - r0;',
    "__mulvsi3": 's32 p; if (__builtin_mul_overflow((s32)r0, (s32)r1, &p)) trap_unsupported(0, "__mulvsi3 overflow"); return (u32)p;',
    "__aeabi_ldivmod":  "u32 o[4]; rt_divmod64(1, r0, r1, r2, r3, o); return PACK(o[0], o[1]);",
    "__aeabi_uldivmod": "u32 o[4]; rt_divmod64(0, r0, r1, r2, r3, o); return PACK(o[0], o[1]);",
    "__clear_cache": "return 0;",
    "__eprintf": 'trap_unsupported(0, "__eprintf (assertion failure)");',
}
INT_ONLY = {"__udivsi3", "__umodsi3", "__divsi3", "__modsi3", "__absvsi2", "__negvsi2", "__mulvsi3",
            "__aeabi_ldivmod", "__aeabi_uldivmod", "__clear_cache", "__aeabi_ui2d", "__floatsidf", "__aeabi_ul2d",
            "__floatdidf", "__aeabi_ui2f", "__floatsisf"}
