#!/usr/bin/env python3
"""
Prototype so-loader for libDeadSpace.so (ARMv5TE, Android) running on Unicorn.

Goal of this prototype: prove the approach before writing the C++/Windows host.
  1. map + relocate the ELF
  2. resolve every import to an HLE stub (libc / pthread / GLES / JNI)
  3. run static constructors (.init_array)
  4. run JNI_OnLoad and dump the natives it registers (the Java<->native surface)
"""
import struct, sys, collections
from unicorn import *
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection

SO = sys.argv[1] if len(sys.argv) > 1 else "../apk/lib/armeabi/libDeadSpace.so"

LIB_BASE   = 0x10000000
HEAP_BASE  = 0x20000000
HEAP_SIZE  = 0x10000000          # 256 MB virtual, committed lazily by the host
DATA_BASE  = 0x50000000          # imported data objects (__sF, ctype tables, ...)
JNI_BASE   = 0x58000000
MMAP_BASE  = 0x30000000
SBRK_BASE  = 0x40000000
STUB_BASE  = 0x70000000
STUB_SIZE  = 0x00100000
STACK_TOP  = 0x7f000000
STACK_SIZE = 0x00800000
EXIT_STUB  = STUB_BASE           # emu_start(..., until=EXIT_STUB)
RESUME_STUB= STUB_BASE + 4       # continuation trampoline for host->guest calls

R_ARM_ABS32, R_ARM_GLOB_DAT, R_ARM_JUMP_SLOT, R_ARM_RELATIVE = 2, 21, 22, 23

# --------------------------------------------------------------------------- JNI tables
def _jni_names():
    n = ["reserved0", "reserved1", "reserved2", "reserved3", "GetVersion", "DefineClass",
         "FindClass", "FromReflectedMethod", "FromReflectedField", "ToReflectedMethod",
         "GetSuperclass", "IsAssignableFrom", "ToReflectedField", "Throw", "ThrowNew",
         "ExceptionOccurred", "ExceptionDescribe", "ExceptionClear", "FatalError",
         "PushLocalFrame", "PopLocalFrame", "NewGlobalRef", "DeleteGlobalRef",
         "DeleteLocalRef", "IsSameObject", "NewLocalRef", "EnsureLocalCapacity",
         "AllocObject", "NewObject", "NewObjectV", "NewObjectA", "GetObjectClass",
         "IsInstanceOf", "GetMethodID"]
    types = ["Object", "Boolean", "Byte", "Char", "Short", "Int", "Long", "Float", "Double", "Void"]
    for t in types: n += [f"Call{t}Method", f"Call{t}MethodV", f"Call{t}MethodA"]
    for t in types: n += [f"CallNonvirtual{t}Method", f"CallNonvirtual{t}MethodV", f"CallNonvirtual{t}MethodA"]
    n += ["GetFieldID"]
    ft = types[:-1]
    n += [f"Get{t}Field" for t in ft] + [f"Set{t}Field" for t in ft]
    n += ["GetStaticMethodID"]
    for t in types: n += [f"CallStatic{t}Method", f"CallStatic{t}MethodV", f"CallStatic{t}MethodA"]
    n += ["GetStaticFieldID"]
    n += [f"GetStatic{t}Field" for t in ft] + [f"SetStatic{t}Field" for t in ft]
    n += ["NewString", "GetStringLength", "GetStringChars", "ReleaseStringChars", "NewStringUTF",
          "GetStringUTFLength", "GetStringUTFChars", "ReleaseStringUTFChars", "GetArrayLength",
          "NewObjectArray", "GetObjectArrayElement", "SetObjectArrayElement"]
    pt = types[1:-1]
    n += [f"New{t}Array" for t in pt] + [f"Get{t}ArrayElements" for t in pt]
    n += [f"Release{t}ArrayElements" for t in pt] + [f"Get{t}ArrayRegion" for t in pt]
    n += [f"Set{t}ArrayRegion" for t in pt]
    n += ["RegisterNatives", "UnregisterNatives", "MonitorEnter", "MonitorExit", "GetJavaVM",
          "GetStringRegion", "GetStringUTFRegion", "GetPrimitiveArrayCritical",
          "ReleasePrimitiveArrayCritical", "GetStringCritical", "ReleaseStringCritical",
          "NewWeakGlobalRef", "DeleteWeakGlobalRef", "ExceptionCheck", "NewDirectByteBuffer",
          "GetDirectBufferAddress", "GetDirectBufferCapacity", "GetObjectRefType"]
    return n
JNI_NAMES = _jni_names()
JVM_NAMES = ["reserved0", "reserved1", "reserved2", "DestroyJavaVM", "AttachCurrentThread",
             "DetachCurrentThread", "GetEnv", "AttachCurrentThreadAsDaemon"]

DATA_SYMBOLS = {"__sF": 3 * 88, "__stack_chk_guard": 4, "_ctype_": 257 + 16, "_tolower_tab_": 4 * 257 + 16,
                "_toupper_tab_": 4 * 257 + 16, "__page_size": 4, "__dso_handle": 4, "tzname": 8}


class Emu:
    def __init__(self, path):
        self.f = open(path, "rb")
        self.elf = ELFFile(self.f)
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.stubs = {}                 # addr -> name
        self.stub_by_name = {}
        self.next_stub = STUB_BASE + 0x10
        self.next_data = DATA_BASE
        self.handlers = {}
        self.heap_ptr = HEAP_BASE + 0x1000
        self.sizes = {}                 # malloc'd block sizes
        self.unhandled = collections.Counter()
        self.handles = {}               # JNI handle -> description
        self.next_handle = 0x1000
        self.natives = []               # (class, name, sig, fnptr)
        self.tls = {}
        self.next_key = 1
        self.resume_stack = []
        self.log_calls = False
        self.trace = None
        self.exports = {}
        self._setup()

    # ------------------------------------------------------------------ memory helpers
    def rd(self, a, n): return bytes(self.uc.mem_read(a, n))
    def wr(self, a, b): self.uc.mem_write(a, bytes(b))
    def u32(self, a): return struct.unpack("<I", self.rd(a, 4))[0]
    def w32(self, a, v): self.wr(a, struct.pack("<I", v & 0xffffffff))
    def cstr(self, a, limit=4096):
        out = bytearray()
        while len(out) < limit:
            c = self.rd(a + len(out), 1)[0]
            if c == 0: break
            out.append(c)
        return out.decode("latin1")
    def reg(self, r): return self.uc.reg_read(r)
    def arg(self, i):
        if i < 4: return self.reg([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][i])
        return self.u32(self.reg(UC_ARM_REG_SP) + 4 * (i - 4))
    def ret(self, v): self.uc.reg_write(UC_ARM_REG_R0, v & 0xffffffff)

    # ------------------------------------------------------------------ setup
    def _setup(self):
        uc = self.uc
        # enable VFP (the library is VFPv2 softfp)
        uc.reg_write(UC_ARM_REG_C1_C0_2, uc.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))
        uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        uc.mem_map(HEAP_BASE, HEAP_SIZE)
        uc.mem_map(DATA_BASE, 0x100000)
        uc.mem_map(MMAP_BASE, 0x10000000)
        uc.mem_map(SBRK_BASE, 0x10000000)
        self.mmap_ptr = MMAP_BASE; self.brk = SBRK_BASE
        uc.mem_map(JNI_BASE, 0x10000)
        uc.mem_map(STUB_BASE, STUB_SIZE)
        uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE)
        # every stub is a `bx lr`; handlers run from a code hook before it executes
        for a in range(STUB_BASE, STUB_BASE + STUB_SIZE, 4):
            pass
        uc.mem_write(STUB_BASE, struct.pack("<I", 0xE12FFF1E) * (STUB_SIZE // 4))
        uc.hook_add(UC_HOOK_CODE, self._stub_hook, begin=STUB_BASE, end=STUB_BASE + STUB_SIZE - 1)
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._unmapped)
        self._load_elf()
        self._build_jni()

    def _load_elf(self):
        elf, uc = self.elf, self.uc
        top = max(s["p_vaddr"] + s["p_memsz"] for s in elf.iter_segments() if s["p_type"] == "PT_LOAD")
        top = (top + 0xFFF) & ~0xFFF
        uc.mem_map(LIB_BASE, top)
        for seg in elf.iter_segments():
            if seg["p_type"] != "PT_LOAD": continue
            uc.mem_write(LIB_BASE + seg["p_vaddr"], seg.data())
        self.lib_end = LIB_BASE + top
        dynsym = elf.get_section_by_name(".dynsym")
        for sym in dynsym.iter_symbols():
            if sym["st_shndx"] != "SHN_UNDEF" and sym.name:
                self.exports[sym.name] = LIB_BASE + sym["st_value"]
        for secname in (".rel.dyn", ".rel.plt"):
            sec = elf.get_section_by_name(secname)
            if not isinstance(sec, RelocationSection): continue
            for r in sec.iter_relocations():
                off = LIB_BASE + r["r_offset"]
                t = r["r_info_type"]
                if t == R_ARM_RELATIVE:
                    self.w32(off, self.u32(off) + LIB_BASE)
                    continue
                sym = dynsym.get_symbol(r["r_info_sym"])
                if sym["st_shndx"] != "SHN_UNDEF":
                    s = LIB_BASE + sym["st_value"]
                else:
                    s = self.resolve_import(sym.name)
                if t in (R_ARM_GLOB_DAT, R_ARM_JUMP_SLOT): self.w32(off, s)
                elif t == R_ARM_ABS32: self.w32(off, s + self.u32(off))
                else: print(f"!! unhandled reloc type {t} for {sym.name}")

    def resolve_import(self, name):
        if name in DATA_SYMBOLS or name.startswith(("_ZTV", "_ZTI", "_ZTS")):
            if name in self.stub_by_name: return self.stub_by_name[name]
            size = DATA_SYMBOLS.get(name, 0x100)
            a = self.next_data; self.next_data += (size + 15) & ~15
            self.stub_by_name[name] = a
            if name == "__stack_chk_guard": self.w32(a, 0xA5A5C0DE)
            if name == "__page_size": self.w32(a, 4096)
            if name in ("_ctype_", "_tolower_tab_", "_toupper_tab_"):       # bionic: pointer variables to the tables
                tbl = a
                for c in range(-1, 256):
                    if name == "_ctype_":
                        f = 0
                        if 65 <= c <= 90: f |= 1
                        if 97 <= c <= 122: f |= 2
                        if 48 <= c <= 57: f |= 4
                        if 9 <= c <= 13 or c == 32: f |= 8
                        if 32 < c < 127 and not f & 7: f |= 16
                        if 0 <= c < 32 or c == 127: f |= 32
                        if 48 <= c <= 57 or 97 <= c <= 102 or 65 <= c <= 70: f |= 64
                        if c == 32: f |= 128
                        if c >= 0: self.wr(tbl + c + 1, bytes([f]))
                    else:
                        v = c if c < 0 else ((c - 32 if 97 <= c <= 122 else c) if name == "_toupper_tab_" else (c + 32 if 65 <= c <= 90 else c))
                        self.wr(tbl + 2 * (c + 1), struct.pack("<h", v))
                cell = self.next_data; self.next_data += 16
                self.w32(cell, tbl); self.stub_by_name[name] = cell
                return cell
            return a
        return self.stub_for(name)

    def stub_for(self, name):
        if name in self.stub_by_name: return self.stub_by_name[name]
        a = self.next_stub; self.next_stub += 4
        self.stub_by_name[name] = a; self.stubs[a] = name
        return a

    def _build_jni(self):
        env, vm, ftab, vtab = JNI_BASE, JNI_BASE + 0x10, JNI_BASE + 0x100, JNI_BASE + 0x800
        self.env, self.vm = env, vm
        self.w32(env, ftab); self.w32(vm, vtab)
        for i, n in enumerate(JNI_NAMES): self.w32(ftab + 4 * i, self.stub_for("JNI:" + n))
        for i, n in enumerate(JVM_NAMES): self.w32(vtab + 4 * i, self.stub_for("JVM:" + n))

    # ------------------------------------------------------------------ dispatch
    def _unmapped(self, uc, access, addr, size, value, ud):
        print(f"!! unmapped access addr={addr:#x} size={size} pc={self.reg(UC_ARM_REG_PC):#x} "
              f"lr={self.reg(UC_ARM_REG_LR):#x}")
        return False

    def _stub_hook(self, uc, addr, size, ud):
        if addr == EXIT_STUB:
            uc.emu_stop(); return
        if addr == RESUME_STUB:
            saved = self.resume_stack.pop()
            for r, v in saved["regs"].items(): uc.reg_write(r, v)
            return
        name = self.stubs.get(addr)
        if name is None: return
        if self.trace is not None: self.trace.append(name)
        h = self.handlers.get(name)
        if self.log_calls: print(f"   call {name}")
        if h is None:
            if self.unhandled[name] == 0:
                print(f"   [unhandled import] {name}")
            self.unhandled[name] += 1
            self.ret(0)
        else:
            h(self)

    # ------------------------------------------------------------------ running guest code
    def call(self, addr, *args, max_insns=200_000_000):
        uc = self.uc
        regs = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
        for r, v in zip(regs, args): uc.reg_write(r, v & 0xffffffff)
        uc.reg_write(UC_ARM_REG_SP, STACK_TOP - 0x100)
        uc.reg_write(UC_ARM_REG_LR, EXIT_STUB)
        uc.emu_start(addr, EXIT_STUB, count=max_insns)
        return self.reg(UC_ARM_REG_R0)

    def call_from_hook(self, fn, args, result_after):
        """Make guest function `fn` run *now*, then resume the interrupted caller with r0=result_after."""
        uc = self.uc
        names = [UC_ARM_REG_R0 + i for i in range(13)] + [UC_ARM_REG_SP, UC_ARM_REG_LR]
        saved = {r: self.reg(r) for r in names}
        saved["regs"] = None
        resume_regs = dict((r, saved[r]) for r in names)
        resume_regs[UC_ARM_REG_R0] = result_after
        resume_regs[UC_ARM_REG_PC] = saved[UC_ARM_REG_LR]
        self.resume_stack.append({"regs": resume_regs})
        for i, a in enumerate(args): uc.reg_write(UC_ARM_REG_R0 + i, a)
        uc.reg_write(UC_ARM_REG_LR, RESUME_STUB)
        uc.reg_write(UC_ARM_REG_PC, fn)


# --------------------------------------------------------------------------- HLE library
def H(emu):
    h = emu.handlers

    def alloc(n, zero=False):
        n = (max(n, 1) + 15) & ~15
        a = emu.heap_ptr; emu.heap_ptr += n + 16
        emu.sizes[a] = n
        return a
    def r(i): return emu.arg(i)

    # -- memory
    h["malloc"] = lambda e: e.ret(alloc(r(0)))
    h["calloc"] = lambda e: e.ret(alloc(r(0) * r(1)))
    h["free"] = lambda e: None
    def realloc(e):
        p, n = r(0), r(1); q = alloc(n)
        if p:
            old = min(e.sizes.get(p, n), n); e.wr(q, e.rd(p, old))
        e.ret(q)
    h["realloc"] = realloc
    for n in ("_Znwj", "_Znaj", "_ZnwjRKSt9nothrow_t", "_ZnajRKSt9nothrow_t"):
        h[n] = lambda e: e.ret(alloc(r(0)))
    for n in ("_ZdlPv", "_ZdaPv", "_ZdlPvRKSt9nothrow_t", "_ZdaPvRKSt9nothrow_t"):
        h[n] = lambda e: None
    def memcpy(e): e.wr(r(0), e.rd(r(1), r(2))) if r(2) else None; e.ret(r(0))
    h["memcpy"] = h["memmove"] = h["__aeabi_memcpy"] = h["__aeabi_memcpy4"] = h["__aeabi_memcpy8"] = \
        h["__aeabi_memmove"] = h["__aeabi_memmove4"] = h["__aeabi_memmove8"] = memcpy
    def memset(e): e.wr(r(0), bytes([r(1) & 0xff]) * r(2)) if r(2) else None; e.ret(r(0))
    h["memset"] = memset
    def aeabi_memset(e): e.wr(r(0), bytes([r(2) & 0xff]) * r(1)) if r(1) else None
    h["__aeabi_memset"] = h["__aeabi_memset4"] = h["__aeabi_memset8"] = aeabi_memset
    def aeabi_memclr(e): e.wr(r(0), b"\0" * r(1)) if r(1) else None
    h["__aeabi_memclr"] = h["__aeabi_memclr4"] = h["__aeabi_memclr8"] = aeabi_memclr
    def memcmp(e):
        a, b = e.rd(r(0), r(2)), e.rd(r(1), r(2)); e.ret((a > b) - (a < b))
    h["memcmp"] = memcmp
    # -- strings
    h["strlen"] = lambda e: e.ret(len(e.cstr(r(0), 1 << 20)))
    def strcmp(e):
        a, b = e.cstr(r(0)), e.cstr(r(1)); e.ret((a > b) - (a < b))
    h["strcmp"] = strcmp
    def strncmp(e):
        a, b = e.cstr(r(0))[:r(2)], e.cstr(r(1))[:r(2)]; e.ret((a > b) - (a < b))
    h["strncmp"] = strncmp
    def strcpy(e): e.wr(r(0), e.cstr(r(1), 1 << 20).encode("latin1") + b"\0"); e.ret(r(0))
    h["strcpy"] = strcpy
    def strncpy(e):
        s = e.cstr(r(1), 1 << 20).encode("latin1")[:r(2)]; e.wr(r(0), s.ljust(r(2), b"\0")); e.ret(r(0))
    h["strncpy"] = strncpy
    def strcat(e):
        d = e.cstr(r(0), 1 << 20); e.wr(r(0) + len(d), e.cstr(r(1), 1 << 20).encode("latin1") + b"\0"); e.ret(r(0))
    h["strcat"] = strcat
    def strchr(e):
        s = e.cstr(r(0), 1 << 20); c = chr(r(1) & 0xff); i = s.find(c)
        e.ret(0 if (i < 0 and c != "\0") else r(0) + (len(s) if c == "\0" else i))
    h["strchr"] = strchr
    def strrchr(e):
        s = e.cstr(r(0), 1 << 20); i = s.rfind(chr(r(1) & 0xff)); e.ret(0 if i < 0 else r(0) + i)
    h["strrchr"] = strrchr
    def strstr(e):
        s, n = e.cstr(r(0), 1 << 20), e.cstr(r(1)); i = s.find(n); e.ret(0 if i < 0 else r(0) + i)
    h["strstr"] = strstr
    def strdup(e):
        s = e.cstr(r(0), 1 << 20).encode("latin1") + b"\0"; p = alloc(len(s)); e.wr(p, s); e.ret(p)
    h["strdup"] = strdup
    # -- C++ runtime
    h["__cxa_atexit"] = h["__aeabi_atexit"] = lambda e: e.ret(0)
    def guard_acq(e): e.ret(0 if e.rd(r(0), 1)[0] & 1 else 1)
    def guard_rel(e): e.wr(r(0), b"\x01")
    h["__cxa_guard_acquire"], h["__cxa_guard_release"] = guard_acq, guard_rel
    h["__cxa_guard_abort"] = lambda e: None
    def stk_fail(e): print("!! __stack_chk_fail"); e.uc.emu_stop()
    h["__stack_chk_fail"] = stk_fail
    h["abort"] = lambda e: (print("!! abort() called from lr=%#x" % e.reg(UC_ARM_REG_LR)), e.uc.emu_stop())
    # -- pthread (single-threaded prototype: all locks succeed immediately)
    for n in ("pthread_mutex_init", "pthread_mutex_lock", "pthread_mutex_unlock", "pthread_mutex_destroy",
              "pthread_mutex_trylock", "pthread_mutexattr_init", "pthread_mutexattr_destroy",
              "pthread_mutexattr_settype", "pthread_mutexattr_setpshared", "pthread_cond_init",
              "pthread_cond_signal", "pthread_cond_broadcast", "pthread_cond_destroy",
              "pthread_attr_init", "pthread_attr_destroy", "pthread_attr_setstacksize",
              "pthread_attr_setdetachstate", "sem_init", "sem_post", "sem_destroy"):
        h[n] = lambda e: e.ret(0)
    def key_create(e):
        k = e.next_key; e.next_key += 1; e.w32(r(0), k); e.ret(0)
    h["pthread_key_create"] = key_create
    h["pthread_getspecific"] = lambda e: e.ret(e.tls.get(r(0), 0))
    def setspecific(e): e.tls[r(0)] = r(1); e.ret(0)
    h["pthread_setspecific"] = setspecific
    h["pthread_self"] = lambda e: e.ret(1)
    def once(e):
        ctl, fn = r(0), r(1)
        if e.u32(ctl) == 0:
            e.w32(ctl, 1); e.call_from_hook(fn, [], 0)
        else: e.ret(0)
    h["pthread_once"] = once
    # -- bionic atomics
    def a_inc(e): v = e.u32(r(0)); e.w32(r(0), v + 1); e.ret(v)
    def a_dec(e): v = e.u32(r(0)); e.w32(r(0), v - 1); e.ret(v)
    def a_swap(e): v = e.u32(r(1)); e.w32(r(1), r(0)); e.ret(v)
    def a_cmpxchg(e):
        v = e.u32(r(2))
        if v == r(0): e.w32(r(2), r(1)); e.ret(0)
        else: e.ret(1)
    h["__atomic_inc"], h["__atomic_dec"], h["__atomic_swap"], h["__atomic_cmpxchg"] = a_inc, a_dec, a_swap, a_cmpxchg
    # -- address space
    def mmap(e):
        ln, flags, fd = (r(1) + 0xFFF) & ~0xFFF, r(3), r(4)
        if not (flags & 0x20) and fd != 0xFFFFFFFF:      # file-backed: not supported yet
            print("   [mmap file-backed fd=%d]" % fd); e.ret(0xFFFFFFFF); return
        a = e.mmap_ptr; e.mmap_ptr += ln; e.ret(a)
    h["mmap"] = mmap
    h["munmap"] = lambda e: e.ret(0)
    def sbrk(e):
        inc = r(0); inc = inc - (1 << 32) if inc & 0x80000000 else inc
        old = e.brk; e.brk += inc; e.ret(old)
    h["sbrk"] = sbrk
    # -- misc
    h["__errno"] = lambda e: e.ret(e.resolve_import("__page_size") + 0)  # scratch int
    h["__android_log_print"] = lambda e: print("   [android_log]", e.cstr(r(2)))
    h["__android_log_write"] = lambda e: print("   [android_log]", e.cstr(r(2)), "|", e.cstr(r(3)))
    h["sysconf"] = lambda e: e.ret(4096 if r(0) == 39 else 1)
    h["getenv"] = lambda e: e.ret(0)
    h["clock_gettime"] = lambda e: (e.w32(r(1), 1), e.w32(r(1) + 4, 0), e.ret(0))
    h["time"] = lambda e: e.ret(1_700_000_000)

    # -- JNI: just enough for JNI_OnLoad
    def new_handle(desc):
        e = emu; hd = e.next_handle; e.next_handle += 8; e.handles[hd] = desc; return hd
    h["JVM:GetEnv"] = lambda e: (e.w32(r(1), e.env), e.ret(0))
    h["JNI:GetVersion"] = lambda e: e.ret(0x10006)
    def find_class(e):
        n = e.cstr(r(1)); e.ret(new_handle(("class", n)))
    h["JNI:FindClass"] = find_class
    for n in ("NewGlobalRef", "NewLocalRef", "NewWeakGlobalRef"):
        h["JNI:" + n] = lambda e: e.ret(r(1))
    for n in ("DeleteLocalRef", "DeleteGlobalRef", "ExceptionClear", "DeleteWeakGlobalRef"):
        h["JNI:" + n] = lambda e: None
    h["JNI:ExceptionCheck"] = h["JNI:ExceptionOccurred"] = lambda e: e.ret(0)
    def mid(kind):
        def f(e):
            cls = e.handles.get(r(1), ("class", "?"))[1]
            d = (kind, cls, e.cstr(r(2)), e.cstr(r(3)))
            print(f"   JNI {kind:<8} {cls}.{d[2]} {d[3]}")
            e.ret(new_handle(d))
        return f
    h["JNI:GetStaticMethodID"] = mid("static"); h["JNI:GetMethodID"] = mid("method")
    h["JNI:GetFieldID"] = mid("field"); h["JNI:GetStaticFieldID"] = mid("sfield")
    def reg_natives(e):
        cls = e.handles.get(r(1), ("class", "?"))[1]; arr, n = r(2), r(3)
        for i in range(n):
            nm, sg, fn = e.u32(arr + 12 * i), e.u32(arr + 12 * i + 4), e.u32(arr + 12 * i + 8)
            e.natives.append((cls, e.cstr(nm), e.cstr(sg), fn))
        e.ret(0)
    h["JNI:RegisterNatives"] = reg_natives


def main():
    emu = Emu(SO)
    H(emu)
    ndef = len([n for n in emu.stubs.values() if not n.startswith(("JNI:", "JVM:"))])
    print(f"loaded {SO}: base={LIB_BASE:#x} end={emu.lib_end:#x}, {ndef} import stubs, "
          f"{len(emu.exports)} exports")

    init_sec = emu.elf.get_section_by_name(".init_array")
    n_init = init_sec["sh_size"] // 4
    print(f"running {n_init} static constructors")
    for i in range(n_init):
        fn = emu.u32(LIB_BASE + init_sec["sh_addr"] + 4 * i)
        if fn in (0, 0xffffffff): continue
        try:
            emu.call(fn)
        except UcError as ex:
            print(f"!! constructor {i} @ {fn:#x} failed: {ex}"); break

    onload = emu.exports.get("JNI_OnLoad")
    print(f"calling JNI_OnLoad @ {onload:#x}")
    try:
        v = emu.call(onload, emu.vm, 0)
        print(f"JNI_OnLoad returned {v:#x}")
    except UcError as ex:
        print("!! JNI_OnLoad failed:", ex, f"pc={emu.reg(UC_ARM_REG_PC):#x}")

    print("\nRegistered natives:")
    for c, n, s, fn in emu.natives: print(f"  {c}.{n}{s}  -> {fn:#x}")
    if emu.unhandled:
        print("\nImports hit but not yet implemented:", ", ".join(sorted(emu.unhandled)))

if __name__ == "__main__":
    main()
