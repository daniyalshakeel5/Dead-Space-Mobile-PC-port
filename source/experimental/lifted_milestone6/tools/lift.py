#!/usr/bin/env python3
"""
ARM-state -> C lifter for libDeadSpace.so (phase 1: integer ISA, loads/stores, branches).

Generated function convention (matches rt.h):
    u64 f_XXXXXXXX(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp)   // returns r0 | r1<<32
Registers r4-r12/lr and the flags are C locals; memory is real memory (guest ptr == host ptr).
PC-relative values are `g_base + vaddr`, so the image can be loaded at any address.
"""
import struct, collections
from elftools.elf.elffile import ELFFile

M32 = 0xFFFFFFFF
NAMES = [f"r{i}" for i in range(13)] + ["sp", "lr"]
COND = ["fZ", "!fZ", "fC", "!fC", "fN", "!fN", "fV", "!fV", "(fC&&!fZ)", "(!fC||fZ)",
        "(fN==fV)", "(fN!=fV)", "(!fZ&&fN==fV)", "(fZ||fN!=fV)", None, "0"]
RET = "return PACK(r0, r1);"
ARGS = "r0, r1, r2, r3, sp"


class Unsupported(Exception):
    pass


def ror32(x, n):
    n &= 31
    return ((x >> n) | (x << (32 - n))) & M32 if n else x & M32


def sx(v, bits):
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


class Image:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.elf = ELFFile(self.f)
        top = max(s["p_vaddr"] + s["p_memsz"] for s in self.elf.iter_segments() if s["p_type"] == "PT_LOAD")
        self.mem = bytearray((top + 0xFFF) & ~0xFFF)
        for s in self.elf.iter_segments():
            if s["p_type"] == "PT_LOAD":
                d = s.data(); self.mem[s["p_vaddr"]: s["p_vaddr"] + len(d)] = d
        text = self.elf.get_section_by_name(".text")
        self.text = (text["sh_addr"], text["sh_addr"] + text["sh_size"])
        self.funcs = {}          # arm-state entry -> (size, name)
        self.thumb = {}
        for s in self.elf.get_section_by_name(".symtab").iter_symbols():
            if s["st_info"]["type"] == "STT_FUNC" and s["st_size"] > 0:
                v = s["st_value"]
                if not (self.text[0] <= (v & ~1) < self.text[1]): continue
                if v & 1: self.thumb[v & ~1] = (s["st_size"], s.name)
                else: self.funcs.setdefault(v, (s["st_size"], s.name))
        self.plt = self._parse_plt()
        from overrides import OVERRIDES
        self.names_at = {}                         # every FUNC symbol, any size -> name
        for sy in self.elf.get_section_by_name(".symtab").iter_symbols():
            if sy["st_info"]["type"] == "STT_FUNC" and self.text[0] <= (sy["st_value"] & ~1) < self.text[1]:
                self.names_at.setdefault(sy["st_value"] & ~1, sy.name)
        self.override = {a: n for a, n in self.names_at.items() if n in OVERRIDES}
        for a in self.override: self.funcs.pop(a, None)

    def w(self, a):
        return struct.unpack_from("<I", self.mem, a)[0]

    def _parse_plt(self):
        plt = self.elf.get_section_by_name(".plt")
        got_names = {}
        dynsym = self.elf.get_section_by_name(".dynsym")
        for secname in (".rel.plt",):
            sec = self.elf.get_section_by_name(secname)
            for r in sec.iter_relocations():
                got_names[r["r_offset"]] = dynsym.get_symbol(r["r_info_sym"]).name
        out = {}
        a = plt["sh_addr"] + 20                      # skip PLT0
        end = plt["sh_addr"] + plt["sh_size"]
        while a + 12 <= end:
            w0, w1, w2 = self.w(a), self.w(a + 4), self.w(a + 8)
            if (w0 & 0xFFFFF000) == 0xE28FC000 and (w1 & 0xFFFFF000) == 0xE28CC000 and (w2 & 0xFFFFF000) == 0xE5BCF000:
                ip = a + 8 + ror32(w0 & 0xFF, ((w0 >> 8) & 0xF) * 2)
                ip += ror32(w1 & 0xFF, ((w1 >> 8) & 0xF) * 2)
                slot = ip + (w2 & 0xFFF)
                if slot in got_names: out[a] = got_names[slot]
            a += 12
        return out


class FuncLifter:
    def __init__(self, img, start):
        self.img, self.start = img, start
        self.size, self.name = img.funcs[start]
        self.end = start + self.size
        self.out = {}                    # addr -> [C lines]
        self.labels = set()
        self.calls = set()
        self.indirect = False
        self.nwords = 0
        self.uses_vfp = False
        self.traps = []

    # ---------------------------------------------------------------- helpers
    def R(self, n, a, shreg=False):
        if n == 15: return f"(g_base+{(a + (12 if shreg else 8)) & M32}u)"
        return NAMES[n]

    def callee(self, t):
        if t in self.img.funcs or t in self.img.override: name = f"f_{t:08x}"
        elif t in self.img.plt: name = f"imp_{self.img.plt[t]}"
        else: raise Unsupported(f"call to non-function {t:x}")
        self.calls.add(name)
        return name

    @staticmethod
    def wrap(c, lines):
        if c is None: return lines
        if len(lines) == 1 and not lines[0].startswith("{"): return [f"if ({c}) {lines[0]}"]
        return [f"if ({c}) {{"] + ["  " + l for l in lines] + ["}"]

    def shifter(self, w, a):
        """Returns (value_expr, carry_expr or None meaning 'carry unchanged')."""
        if w & (1 << 25):
            rot = ((w >> 8) & 0xF) * 2
            val = ror32(w & 0xFF, rot)
            return f"{val}u", (None if rot == 0 else str(val >> 31))
        rm, st = w & 0xF, (w >> 5) & 3
        if w & 0x10:
            rs = (w >> 8) & 0xF
            if rs == 15 or rm == 15: raise Unsupported("pc in register-shifted operand")
            x, n = NAMES[rm], f"({NAMES[rs]}&255)"
            f = ["lsl", "lsr", "asr", "ror"][st]
            return f"sh_{f}({x},{n})", f"sh_{f}_c({x},{n},fC)"
        x, amt = self.R(rm, a), (w >> 7) & 0x1F
        if st == 0:
            return (x, None) if amt == 0 else (f"({x}<<{amt})", f"(({x}>>{32 - amt})&1)")
        if st == 1:
            n = amt or 32
            return ("0" if n == 32 else f"({x}>>{n})"), f"(({x}>>{n - 1})&1)"
        if st == 2:
            n = amt or 32
            return f"((u32)((s32){x}>>{31 if n == 32 else n}))", f"(({x}>>{n - 1})&1)"
        if amt == 0: return f"((fC<<31)|({x}>>1))", f"({x}&1)"          # RRX
        return f"sh_ror({x},{amt})", f"(({x}>>{amt - 1})&1)"

    # ---------------------------------------------------------------- driver
    def lift(self):
        work = [self.start]
        while work:
            a = work.pop()
            while a not in self.out:
                if not (self.start <= a < self.end):
                    if a in self.img.funcs or a in self.img.override:
                        self.out[a] = [f"return f_{a:08x}({ARGS});"]; self.calls.add(f"f_{a:08x}")
                    else:
                        self.out[a] = [f'trap_unsupported(0x{a:x}u, "flow leaves function");']; self.traps.append((a, "flow leaves function"))
                    break
                try:
                    lines, succ, fall = self.insn(a, self.img.w(a))
                except (Unsupported, IndexError) as e:
                    r = (str(e) if isinstance(e, Unsupported) else 'register pc where not allowed').replace('"', "'")[:70]
                    lines, succ, fall = [f'trap_unsupported(0x{a:x}u, "{r}");'], [], False
                    self.traps.append((a, r))
                self.out[a] = lines
                self.nwords += 1
                for s in succ:
                    self.labels.add(s); work.append(s)
                if not fall: break
                a += 4
            else:
                self.labels.add(a)
        return self.emit()

    def emit(self):
        L = [f"u64 f_{self.start:08x}(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {{ /* {self.name[:90]} */",
             "  u32 r4=0,r5=0,r6=0,r7=0,r8=0,r9=0,r10=0,r11=0,r12=0,lr=0;",
             "  u32 fN=0,fZ=0,fC=0,fV=0; (void)r4;(void)r5;(void)r6;(void)r7;(void)r8;(void)r9;(void)r10;(void)r11;(void)r12;(void)lr;(void)fN;(void)fZ;(void)fC;(void)fV;"]
        # VFP state belongs to the guest CPU, not to a C function invocation.
        # Internal compiler clones can pass arguments in s0 even in this
        # otherwise soft-float ELF (btRayAabb2's maximum ray distance does).
        # Guest vpush/vpop instructions preserve callee-saved registers.
        if self.uses_vfp: L.append("  u32 *V = ds_arm_vfp;")
        for a in sorted(self.out):
            if a in self.labels: L.append(f"L_{a:x}:;")
            L += ["  " + l.replace("fpscr", "ds_arm_fpscr") for l in self.out[a]]
        L.append("}")
        return "\n".join(L)

    # ---------------------------------------------------------------- instruction decode
    def insn(self, a, w):
        cond = w >> 28
        if cond == 0xF:
            if (w & 0xFD70F000) == 0xF550F000: return [], [], True          # pld
            raise Unsupported("cond=NV (blx imm / thumb)")
        c = COND[cond]
        if (w & 0x0FFFFFFF) == 0x01A0E00F:                                  # mov lr, pc ...
            nxt = self.img.w(a + 4)
            call = self.indirect_call_pair(a, nxt) if (nxt >> 28) == cond else None
            if call:                                                        # ... followed by a jump = indirect call
                self.indirect = True
                self.out[a + 4] = []
                return self.wrap(c, call), [a + 8], False
        kind = (w >> 25) & 7
        if kind in (0, 1): lines, succ, fall = self.group0(a, w, kind)
        elif kind in (2, 3):
            if kind == 3 and (w & 0x10): raise Unsupported("media/undefined")
            lines, succ, fall = self.ldst(a, w)
        elif kind == 4: lines, succ, fall = self.ldmstm(a, w)
        elif kind == 5: lines, succ, fall = self.branch(a, w, c)
        else:
            if (w & 0x0F000000) == 0x0F000000: raise Unsupported("swi")
            lines, succ, fall = self.vfp(a, w, kind)
        if kind == 5: return lines, succ, fall                              # branch() applies cond itself
        returns = any(l.strip().startswith("return") for l in lines)
        return self.wrap(c, lines), succ, (fall or (c is not None))


    def indirect_call_pair(self, a, nxt):
        """`mov lr, pc` + (`ldr pc, [..]` | `mov pc, rm` | `bx rm`) is the classic ARM indirect call."""
        call = lambda tgt: [f"{{ u64 rv = call_ind({tgt}, {ARGS}); r0 = (u32)rv; r1 = (u32)(rv>>32); }}"]
        if (nxt & 0x0FFFFFF0) in (0x01A0F000, 0x012FFF10) and (nxt & 15) != 14:
            return call(self.R(nxt & 15, a + 4))
        if (nxt & 0x0D70F000) == 0x0510F000:                                # ldr pc, [rn, off]  (word, pre-indexed, no writeback)
            I, U, rn = (nxt >> 25) & 1, (nxt >> 23) & 1, (nxt >> 16) & 15
            if I:
                if nxt & 0x10: return None
                off, _ = self.shifter(nxt & ~(1 << 25), a + 4)
            else:
                off = f"{nxt & 0xFFF}u"
            base = self.R(rn, a + 4)
            return call(f"LD32({base} {'+' if U else '-'} {off})")
        return None


    # ---- VFPv2 (cp10 = single, cp11 = double)
    def vfp(self, a, w, kind):
        cp = (w >> 8) & 0xF
        if cp not in (10, 11): raise Unsupported("coprocessor")
        self.uses_vfp = True
        dbl = cp == 11
        D, N, M = (w >> 22) & 1, (w >> 7) & 1, (w >> 5) & 1
        Fd, Fn, Fm = (w >> 12) & 15, (w >> 16) & 15, w & 15
        S = lambda f, x: (f << 1) | x
        Dr = lambda f, x: (x << 4) | f
        def F32(i): return f"u2f(V[{i}])"
        def D64(i): return f"u2d(V[{2 * i}],V[{2 * i + 1}])"
        def set32(i, e): return f"V[{i}] = f2ur({e});"
        def set64(i, e): return f"{{ double r = {e}; d2ur(r, &V[{2 * i}], &V[{2 * i + 1}]); }}"
        if kind == 6:
            if (w & 0x0FE00000) == 0x0C400000:                                  # vmov Rt,Rt2 <-> Dm / Sm,Sm1
                rt, rt2, L = (w >> 12) & 15, (w >> 16) & 15, (w >> 20) & 1
                if 15 in (rt, rt2): raise Unsupported("vmov pair with pc")
                base = 2 * Dr(Fm, M) if dbl else S(Fm, M)
                if L: return [f"{NAMES[rt]} = V[{base}]; {NAMES[rt2]} = V[{base + 1}];"], [], True
                return [f"V[{base}] = {NAMES[rt]}; V[{base + 1}] = {NAMES[rt2]};"], [], True
            P, U, W, Lb = (w >> 24) & 1, (w >> 23) & 1, (w >> 21) & 1, (w >> 20) & 1
            rn, imm = (w >> 16) & 15, w & 0xFF
            first = 2 * Dr(Fd, D) if dbl else S(Fd, D)
            base = self.R(rn, a)
            if P and not W:                                                      # vldr / vstr
                ea = f"{base} {'+' if U else '-'} {imm * 4}"
                nw = 2 if dbl else 1
                if Lb: L = [f"V[{first + k}] = LD32(({ea}) + {4 * k});" for k in range(nw)]
                else: L = [f"ST32(({ea}) + {4 * k}, V[{first + k}]);" for k in range(nw)]
                return ["{"] + ["  " + l for l in L] + ["}"], [], True
            if (not P and U) or (P and not U and W):                             # vldm / vstm (IA, DB)
                if rn == 15 or imm == 0: raise Unsupported("vldm/vstm base pc / empty")
                if first + imm > 32: raise Unsupported("vldm/vstm register range")
                b = NAMES[rn]
                L = [f"u32 ea = {b}{' - ' + str(4 * imm) if P else ''};"]
                for k in range(imm):
                    L.append(f"V[{first + k}] = LD32(ea + {4 * k});" if Lb else f"ST32(ea + {4 * k}, V[{first + k}]);")
                if W: L.append(f"{b} = {'ea' if P else b + ' + ' + str(4 * imm)};")
                return ["{"] + ["  " + l for l in L] + ["}"], [], True
            raise Unsupported("vfp ldc/stc form")
        if w & 0x10:                                                              # register transfers
            if (w & 0x0FE00F7F) == 0x0E000A10:                                    # vmov Sn <-> Rt
                rt, i = (w >> 12) & 15, S(Fn, N)
                if rt == 15: raise Unsupported("vmov to/from pc")
                return ([f"{NAMES[rt]} = V[{i}];"] if (w >> 20) & 1 else [f"V[{i}] = {NAMES[rt]};"]), [], True
            if (w & 0x0FEF0FFF) == 0x0EE10A10:                                    # vmrs / vmsr fpscr
                rt = (w >> 12) & 15
                if (w >> 20) & 1:
                    if rt == 15: return ["fN = (fpscr>>31)&1; fZ = (fpscr>>30)&1; fC = (fpscr>>29)&1; fV = (fpscr>>28)&1;"], [], True
                    return [f"{NAMES[rt]} = fpscr;"], [], True
                if rt == 15: raise Unsupported("vmsr pc")
                return [f"fpscr = {NAMES[rt]};"], [], True
            raise Unsupported("vfp register transfer form")
        p, q, r, sbit = (w >> 23) & 1, (w >> 21) & 1, (w >> 20) & 1, (w >> 6) & 1
        if dbl:
            d, n, m = Dr(Fd, D), Dr(Fn, N), Dr(Fm, M)
            X, rd_, setr = D64, d, set64
        else:
            d, n, m = S(Fd, D), S(Fn, N), S(Fm, M)
            X, rd_, setr = F32, d, set32
        key = (p, q, r)
        arith = {(0, 0, 0): ("{D} + ({N} * {M})", "{D} - ({N} * {M})"),
                 (0, 0, 1): ("(-{D}) + ({N} * {M})", "(-{D}) - ({N} * {M})"),
                 (0, 1, 0): ("{N} * {M}", "-({N} * {M})"),
                 (0, 1, 1): ("{N} + {M}", "{N} - {M}"),
                 (1, 0, 0): ("{N} / {M}", None)}
        if key in arith:
            e = arith[key][sbit]
            if e is None: raise Unsupported("vfp data-processing form")
            e = e.format(D=X(d), N=X(n), M=X(m))
            if not dbl and key == (0, 0, 0) or True:
                pass
            return [setr(d, e)], [], True
        if key == (1, 1, 1) and sbit:
            op = Fn
            if op == 0 and not N:                                                 # vmov Vd, Vm
                return ([f"V[{2 * d}] = V[{2 * m}]; V[{2 * d + 1}] = V[{2 * m + 1}];"] if dbl else [f"V[{d}] = V[{m}];"]), [], True
            if op == 0 and N:                                                     # vabs
                return ([f"V[{2 * d}] = V[{2 * m}]; V[{2 * d + 1}] = V[{2 * m + 1}] & 0x7fffffffu;"] if dbl else [f"V[{d}] = V[{m}] & 0x7fffffffu;"]), [], True
            if op == 1 and not N:                                                 # vneg
                return ([f"V[{2 * d}] = V[{2 * m}]; V[{2 * d + 1}] = V[{2 * m + 1}] ^ 0x80000000u;"] if dbl else [f"V[{d}] = V[{m}] ^ 0x80000000u;"]), [], True
            if op == 1 and N:                                                     # vsqrt
                return [setr(d, f"{'sqrt' if dbl else 'sqrtf'}({X(m)})")], [], True
            if op in (4, 5):                                                      # vcmp / vcmpe
                rhs = ("0.0" if dbl else "0.0f") if op == 5 else X(m)
                fn = "vcmp_d" if dbl else "vcmp_f"
                return [f"fpscr = (fpscr & 0x0fffffffu) | ({fn}({X(d)}, {rhs}) << 28);"], [], True
            if op == 7 and N:                                                     # vcvt double <-> single
                if dbl: return [set32(S(Fd, D), f"(float){D64(Dr(Fm, M))}")], [], True
                return [set64(Dr(Fd, D), f"(double){F32(S(Fm, M))}")], [], True
            if op == 8:                                                           # int -> float
                src = f"V[{S(Fm, M)}]"
                conv = f"(s32){src}" if N else f"(u32){src}"
                if dbl: return [set64(Dr(Fd, D), f"(double)({conv})")], [], True
                return [set32(S(Fd, D), f"(float)({conv})")], [], True
            if op in (12, 13):                                                    # float -> int
                signed = op & 1
                if dbl: e = f"{'cvt_d2s' if signed else 'cvt_d2u'}({'' if N else 'rint'}({D64(Dr(Fm, M))}))"
                else: e = f"{'cvt_f2s' if signed else 'cvt_f2u'}({'' if N else 'rintf'}({F32(S(Fm, M))}))"
                return [f"V[{S(Fd, D)}] = {e};"], [], True
            raise Unsupported(f"vfp extension opc2={op}")
        raise Unsupported("vfp data-processing form")


    def dsp(self, w):
        """ARMv5TE DSP extension: smulxy/smlaxy/smlalxy/smulwy/smlawy and qadd/qsub/qdadd/qdsub."""
        rd16, rn12, rs, rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
        x, y = (w >> 5) & 1, (w >> 6) & 1
        if (w & 0x0FF00090) == 0x01600080:                                    # smulxy
            return [f"{NAMES[rd16]} = (u32)(hlf({NAMES[rm]}, {x}) * hlf({NAMES[rs]}, {y}));"]
        if (w & 0x0FF00090) == 0x01000080:                                    # smlaxy
            return [f"{NAMES[rd16]} = (u32)(hlf({NAMES[rm]}, {x}) * hlf({NAMES[rs]}, {y})) + {NAMES[rn12]};"]
        if (w & 0x0FF00090) == 0x01400080:                                    # smlalxy
            return [f"{{ s64 p = (s64)hlf({NAMES[rm]}, {x}) * hlf({NAMES[rs]}, {y}); p += (s64)(((u64){NAMES[rd16]} << 32) | {NAMES[rn12]}); {NAMES[rn12]} = (u32)p; {NAMES[rd16]} = (u32)((u64)p >> 32); }}"]
        if (w & 0x0FF000B0) == 0x012000A0:                                    # smulwy
            return [f"{NAMES[rd16]} = (u32)(((s64)(s32){NAMES[rm]} * hlf({NAMES[rs]}, {y})) >> 16);"]
        if (w & 0x0FF000B0) == 0x01200080:                                    # smlawy
            return [f"{NAMES[rd16]} = (u32)(((s64)(s32){NAMES[rm]} * hlf({NAMES[rs]}, {y})) >> 16) + {NAMES[rn12]};"]
        if (w & 0x0F900FF0) == 0x01000050:                                    # qadd qsub qdadd qdsub
            op = (w >> 21) & 3
            a, b = f"(s64)(s32){NAMES[rm]}", f"(s64)(s32){NAMES[rd16]}"       # Rm, Rn (bits 19:16)
            dbl = f"(s64)(s32)sat32(2 * {b})"
            e = {0: f"{a} + {b}", 1: f"{a} - {b}", 2: f"{a} + {dbl}", 3: f"{a} - {dbl}"}[op]
            return [f"{NAMES[rn12]} = sat32({e});"]
        return None

    # ---- data-processing / multiply / misc
    def group0(self, a, w, kind):
        if kind == 0:
            if (w & 0x0FFFFFF0) == 0x012FFF10:                              # BX
                rm = w & 15
                if rm == 14: return [RET], [], False
                self.indirect = True
                return [f"return call_ind({NAMES[rm]}, {ARGS});"], [], False
            if (w & 0x0FFFFFF0) == 0x012FFF30:                              # BLX reg
                self.indirect = True
                rm = w & 15
                return [f"{{ u64 rv = call_ind({NAMES[rm]}, {ARGS}); r0=(u32)rv; r1=(u32)(rv>>32); }}"], [], True
            if (w & 0x0FFF0FF0) == 0x016F0F10:                              # CLZ
                return [f"{NAMES[(w >> 12) & 15]} = clz32({NAMES[w & 15]});"], [], True
            if (w & 0x0FC000F0) == 0x00000090: return self.mul(a, w), [], True
            if (w & 0x0F8000F0) == 0x00800090: return self.mull(a, w), [], True
            if (w & 0x0FB00FF0) == 0x01000090:                              # swp / swpb
                rn, rd, rm = (w >> 16) & 15, (w >> 12) & 15, w & 15
                ty = "u8" if (w >> 22) & 1 else "u32"
                # ARM SWP is indivisible even when used without a libc call.
                return [f"{{ u32 t = __atomic_exchange_n(({ty}*)(uintptr_t){NAMES[rn]}, ({ty}){NAMES[rm]}, __ATOMIC_SEQ_CST); {NAMES[rd]} = t; }}"], [], True
            dsp = self.dsp(w)
            if dsp: return dsp, [], True
            if (w & 0xFFF000F0) == 0xE1200070: raise Unsupported("bkpt")
            if (w & 0x0E000090) == 0x00000090 and ((w >> 5) & 3): return self.ldst_extra(a, w)
            if (w & 0x0F900000) == 0x01000000: raise Unsupported("misc (mrs/msr/dsp/bkpt)")
        elif (w & 0x0F900000) == 0x01000000:
            if (w & 0x0FFFFFFF) == 0x0320F000: return [], [], True          # nop hint
            raise Unsupported("msr imm")
        return self.dataproc(a, w)

    def dataproc(self, a, w):
        op, S, rn, rd = (w >> 21) & 15, (w >> 20) & 1, (w >> 16) & 15, (w >> 12) & 15
        shreg = not (w & (1 << 25)) and bool(w & 0x10)
        if rd == 15 and op not in (8, 9, 10, 11):
            if op == 13 and not S and not (w & (1 << 25)) and (w & 0xFF0) == 0 and (w & 15) == 14:
                return [RET], [], False                                       # mov pc, lr
            if op == 4 and not S and rn == 15 and not (w & (1 << 25)) and ((w >> 4) & 0xFF) == 0x02 and False:
                pass
            if op == 4 and not S and rn == 15 and not (w & (1 << 25)) and (w & 0xFF0) == 0x100:
                return self.jumptable(a, w)
            raise Unsupported("write to pc")
        v, car = self.shifter(w, a)
        x = self.R(rn, a, shreg)
        logical = op in (0, 1, 8, 9, 12, 13, 14, 15)
        L = []
        if logical:
            e = {0: f"{x}&{v}", 1: f"{x}^{v}", 8: f"{x}&{v}", 9: f"{x}^{v}", 12: f"{x}|{v}", 13: f"{v}",
                 14: f"{x}&~({v})", 15: f"~({v})"}[op]
            L.append(f"u32 t = {e};")
            if S and car is not None: L.append(f"u32 cc = {car};")
            if op not in (8, 9): L.append(f"{NAMES[rd]} = t;")
            if S:
                L.append("fN = t>>31; fZ = (t==0);")
                if car is not None: L.append("fC = cc;")
        else:
            xx, yy, cin = {
                4: (x, v, "0"), 5: (x, v, "fC"), 2: (x, f"~({v})", "1"), 6: (x, f"~({v})", "fC"),
                3: (v, f"~({x})", "1"), 7: (v, f"~({x})", "fC"), 10: (x, v, "0") if False else (x, f"~({v})", "1"),
                11: (x, v, "0")}[op]
            if op == 10: xx, yy, cin = x, f"~({v})", "1"                       # CMP
            L += [f"u32 xx = {xx}, yy = {yy};", f"u64 s = (u64)xx + yy + {cin}; u32 t = (u32)s;"]
            if op not in (10, 11): L.append(f"{NAMES[rd]} = t;")
            if S: L.append("fN = t>>31; fZ = (t==0); fC = (u32)(s>>32); fV = ((xx^t)&(yy^t))>>31;")
        return ["{"] + ["  " + l for l in L] + ["}"], [], True

    def jumptable(self, a, w):
        rm = w & 15
        prev = self.img.w(a - 4)
        if (prev & 0x0FF0F000) != 0x03500000 or ((prev >> 16) & 15) != rm or (prev >> 28) != 9 and False:
            raise Unsupported("jump table without preceding cmp")
        n = ror32(prev & 0xFF, ((prev >> 8) & 0xF) * 2) + 1
        if (w >> 28) != 9: raise Unsupported("jump table with unexpected condition")
        succ = [a + 8 + 4 * i for i in range(n)]
        for s in succ:
            if not (self.start <= s < self.end): raise Unsupported("jump table outside function")
        sw = ["switch (%s) {" % NAMES[rm]] + [f"  case {i}: goto L_{s:x};" for i, s in enumerate(succ)] + ["  default: break;", "}"]
        # handled here (cond folded into the cmp bound), so the caller must not wrap it again
        return ["{"] + ["  " + l for l in sw] + ["}"], succ, True

    def mul(self, a, w):
        rd, rn, rs, rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
        e = f"{NAMES[rm]}*{NAMES[rs]}" + (f"+{NAMES[rn]}" if (w >> 21) & 1 else "")
        L = [f"u32 t = {e}; {NAMES[rd]} = t;"]
        if (w >> 20) & 1: L.append("fN = t>>31; fZ = (t==0);")
        return ["{"] + ["  " + l for l in L] + ["}"]

    def mull(self, a, w):
        signed, acc, S = (w >> 22) & 1, (w >> 21) & 1, (w >> 20) & 1
        hi, lo, rs, rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
        t = "s64" if signed else "u64"
        cast = "s32" if signed else "u32"
        e = f"({t})({cast}){NAMES[rm]} * ({t})({cast}){NAMES[rs]}"
        L = [f"u64 p = (u64)({e});"]
        if acc: L.append(f"p += ((u64){NAMES[hi]}<<32) | {NAMES[lo]};")
        L.append(f"{NAMES[lo]} = (u32)p; {NAMES[hi]} = (u32)(p>>32);")
        if S: L.append("fN = (u32)(p>>63); fZ = (p==0);")
        return ["{"] + ["  " + l for l in L] + ["}"]

    # ---- loads and stores
    def addr_parts(self, a, w, off_expr, rn, P, U, W):
        base = self.R(rn, a)
        sign = "+" if U else "-"
        L = []
        if P:
            L.append(f"u32 ea = {base} {sign} {off_expr};")
            wb = f"{NAMES[rn]} = ea;" if W else None
        else:
            L.append(f"u32 ea = {base};")
            wb = f"{NAMES[rn]} = ea {sign} {off_expr};"
        return L, wb

    def ldst(self, a, w):
        I, P, U, B, W, Lb = (w >> 25) & 1, (w >> 24) & 1, (w >> 23) & 1, (w >> 22) & 1, (w >> 21) & 1, (w >> 20) & 1
        rn, rd = (w >> 16) & 15, (w >> 12) & 15
        if I:
            off, _ = self.shifter(w & ~(1 << 25), a)
            if (w & 0x10): raise Unsupported("register-shift in ldst")
        else:
            off = str(w & 0xFFF) + "u"
        L, wb = self.addr_parts(a, w, off, rn, P, U, W)
        if Lb:
            if rd == 15:
                if rn == 13 and not P and U and not I and (w & 0xFFF) == 4: return [RET], [], False
                raise Unsupported("ldr pc")
            L.append(f"{NAMES[rd]} = {'LD8' if B else 'LD32'}(ea);")
        else:
            val = self.R(rd, a)
            L.append(f"{'ST8' if B else 'ST32'}(ea, {val});")
        if wb and not (Lb and rd == rn): L.append(wb)
        return ["{"] + ["  " + l for l in L] + ["}"], [], True

    def ldst_extra(self, a, w):
        P, U, I, W, Lb = (w >> 24) & 1, (w >> 23) & 1, (w >> 22) & 1, (w >> 21) & 1, (w >> 20) & 1
        rn, rd, sh = (w >> 16) & 15, (w >> 12) & 15, (w >> 5) & 3
        if I: off = str(((w >> 4) & 0xF0) | (w & 0xF)) + "u"
        else:
            if (w >> 8) & 15: raise Unsupported("ldrh reg form with nonzero sbz")
            off = NAMES[w & 15]
        L, wb = self.addr_parts(a, w, off, rn, P, U, W)
        if rd == 15: raise Unsupported("ldrh pc")
        if Lb:
            L.append(f"{NAMES[rd]} = {['', 'LD16', 'LDS8', 'LDS16'][sh]}(ea);")
        elif sh == 1:
            L.append(f"ST16(ea, {self.R(rd, a)});")
        else:                                                                # ldrd / strd
            if rd & 1 or rd == 14: raise Unsupported("ldrd/strd odd register")
            if sh == 2: L.append(f"{NAMES[rd]} = LD32(ea); {NAMES[rd + 1]} = LD32(ea + 4);")
            else: L.append(f"ST32(ea, {NAMES[rd]}); ST32(ea + 4, {NAMES[rd + 1]});")
        if wb and not (Lb and rd == rn): L.append(wb)
        return ["{"] + ["  " + l for l in L] + ["}"], [], True

    def ldmstm(self, a, w):
        P, U, S, W, Lb = (w >> 24) & 1, (w >> 23) & 1, (w >> 22) & 1, (w >> 21) & 1, (w >> 20) & 1
        rn, rl = (w >> 16) & 15, w & 0xFFFF
        regs = [i for i in range(16) if rl >> i & 1]
        n = len(regs)
        if S: raise Unsupported("ldm/stm with S bit")
        if rn == 15 or n == 0: raise Unsupported("ldm/stm base pc / empty list")
        base = NAMES[rn]
        start = {(1, 0): f"{base}", (1, 1): f"{base} + 4", (0, 0): f"{base} - {4 * n - 4}", (0, 1): f"{base} - {4 * n}"}[(U, P)]
        newb = f"{base} {'+' if U else '-'} {4 * n}"
        L = [f"u32 ea = {start};"]
        is_ret = False
        for k, r in enumerate(regs):
            if Lb:
                if r == 15:
                    if rn != 13: raise Unsupported("ldm into pc not via sp")
                    is_ret = True
                else: L.append(f"{NAMES[r]} = LD32(ea + {4 * k});")
            else:
                L.append(f"ST32(ea + {4 * k}, {self.R(r, a)});")
        if W and not (Lb and rn in regs): L.append(f"{base} = {newb};")
        if is_ret: L.append(RET)
        return ["{"] + ["  " + l for l in L] + ["}"], [], not is_ret

    def branch(self, a, w, c):
        link = (w >> 24) & 1
        t = (a + 8 + sx(w & 0xFFFFFF, 24) * 4) & M32
        nm = self.img.names_at.get(t, "")
        import re as _re
        mv = _re.match(r"_(?:interwork_)?(?:r7_|r11_)?call_via_(\w+)$", nm)
        if mv:
            reg = {"sl": "r10", "fp": "r11", "ip": "r12"}.get(mv.group(1), mv.group(1))
            if reg not in NAMES: raise Unsupported(f"call via {reg}")
            self.indirect = True
            if link: return self.wrap(c, [f"{{ u64 rv = call_ind({reg}, {ARGS}); r0 = (u32)rv; r1 = (u32)(rv>>32); }}"]), [], True
            return self.wrap(c, [f"return call_ind({reg}, {ARGS});"]), [], c is not None
        if link and nm in ("__aeabi_ldivmod", "__aeabi_uldivmod"):
            sg = 1 if nm == "__aeabi_ldivmod" else 0
            return self.wrap(c, [f"{{ u32 o[4]; rt_divmod64({sg}, r0, r1, r2, r3, o); r0 = o[0]; r1 = o[1]; r2 = o[2]; r3 = o[3]; }}"]), [], True
        if link and self.img.plt.get(t) in ("setjmp", "_setjmp", "sigsetjmp", "__sigsetjmp"):
            return self.wrap(c, ["r0 = (u32)setjmp(*(jmp_buf*)(uintptr_t)r0);"]), [], True
        if link:
            name = self.callee(t)
            body = [f"{{ u64 rv = {name}({ARGS}); r0 = (u32)rv; r1 = (u32)(rv>>32); }}"]
            return self.wrap(c, body), [], True
        if self.start <= t < self.end:
            return self.wrap(c, [f"goto L_{t:x};"]), [t], c is not None
        name = self.callee(t)
        return self.wrap(c, [f"return {name}({ARGS});"]), [], c is not None


# ------------------------------------------------------------------------- batch driver
def lift_all(img, only=None):
    ok, bad = {}, {}
    for start in sorted(img.funcs):
        if only and start not in only: continue
        if start in img.override: continue
        fl = FuncLifter(img, start)
        try:
            code = fl.lift()
            ok[start] = (code, fl)
        except Unsupported as e:
            bad[start] = str(e)
        except Exception as e:                                                # decoder bug: report loudly
            bad[start] = f"BUG {type(e).__name__}: {e}"
    return ok, bad


if __name__ == "__main__":
    import sys
    img = Image(sys.argv[1] if len(sys.argv) > 1 else "../apk/lib/armeabi/libDeadSpace.so")
    ok, bad = lift_all(img)
    n = len(img.funcs)
    leaves = sum(1 for c, f in ok.values() if not f.calls and not f.indirect)
    print(f"{len(ok)}/{n} ARM functions lifted ({100 * len(ok) / n:.1f}%), {leaves} are leaves, "
          f"{len(img.thumb)} Thumb functions not attempted")
    def key(r):
        if r.startswith("call to non-function"): return "call to non-function (thumb/unknown target)"
        if r.startswith("flow leaves"): return "flow leaves function bounds"
        if r.startswith("BUG"): return r.split(":")[0]
        return r
    hist = collections.Counter(key(r) for r in bad.values())
    for k, v in hist.most_common(25): print(f"  {v:6d}  {k}")
