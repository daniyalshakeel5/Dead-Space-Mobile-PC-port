// dspc - desktop host for libDeadSpace.so (Android ARMv5TE).
//
// Milestone 1: map + relocate the ELF, bind every import to a host stub, run the
// static constructors and JNI_OnLoad. Same behaviour as the Python prototype.
//
// The guest runs on Unicorn. Every import resolves to a 4-byte "bx lr" stub in a
// reserved region; a code hook fires when the PC reaches a stub and runs the host
// handler (reads args from r0-r3/stack, writes r0), then the "bx lr" returns.
#include <unicorn/unicorn.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using u8 = uint8_t;
using u32 = uint32_t;

// ------------------------------------------------------------------ guest address map
constexpr u32 LIB_BASE = 0x10000000, HEAP_BASE = 0x20000000, HEAP_SIZE = 0x10000000;
constexpr u32 MMAP_BASE = 0x30000000, SBRK_BASE = 0x40000000, MAP_SPAN = 0x10000000;
constexpr u32 DATA_BASE = 0x50000000, DATA_SIZE = 0x100000;
constexpr u32 JNI_BASE = 0x58000000, JNI_SIZE = 0x10000;
constexpr u32 TEST_BASE = 0x5a000000, TEST_SIZE = 0x2000;
constexpr u32 STUB_BASE = 0x70000000, STUB_SIZE = 0x100000;
constexpr u32 STACK_TOP = 0x7f000000, STACK_SIZE = 0x800000;
constexpr u32 EXIT_STUB = STUB_BASE, RESUME_STUB = STUB_BASE + 4;

// ELF32 (declared here because <elf.h> does not exist on Windows)
struct Ehdr { u8 ident[16]; uint16_t type, machine; u32 version, entry, phoff, shoff, flags;
              uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx; };
struct Phdr { u32 type, offset, vaddr, paddr, filesz, memsz, flags, align; };
struct Dyn  { int32_t tag; u32 val; };
struct Sym  { u32 name, value, size; u8 info, other; uint16_t shndx; };
struct Rel  { u32 offset, info; };
enum { PT_LOAD = 1, PT_DYNAMIC = 2 };
enum { DT_PLTRELSZ = 2, DT_HASH = 4, DT_STRTAB = 5, DT_SYMTAB = 6, DT_INIT = 12, DT_REL = 17,
       DT_RELSZ = 18, DT_JMPREL = 23, DT_INIT_ARRAY = 25, DT_INIT_ARRAYSZ = 27 };
enum { R_ARM_ABS32 = 2, R_ARM_GLOB_DAT = 21, R_ARM_JUMP_SLOT = 22, R_ARM_RELATIVE = 23 };

static const int REGS[16] = {UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R4,
                             UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8, UC_ARM_REG_R9,
                             UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP,
                             UC_ARM_REG_LR, UC_ARM_REG_PC};
enum { R0, R1, R2, R3, SP = 13, LR = 14, PC = 15 };

// ------------------------------------------------------------------ JNI function table order
static std::vector<std::string> jniNames() {
    std::vector<std::string> n = {"reserved0", "reserved1", "reserved2", "reserved3", "GetVersion",
        "DefineClass", "FindClass", "FromReflectedMethod", "FromReflectedField", "ToReflectedMethod",
        "GetSuperclass", "IsAssignableFrom", "ToReflectedField", "Throw", "ThrowNew",
        "ExceptionOccurred", "ExceptionDescribe", "ExceptionClear", "FatalError", "PushLocalFrame",
        "PopLocalFrame", "NewGlobalRef", "DeleteGlobalRef", "DeleteLocalRef", "IsSameObject",
        "NewLocalRef", "EnsureLocalCapacity", "AllocObject", "NewObject", "NewObjectV", "NewObjectA",
        "GetObjectClass", "IsInstanceOf", "GetMethodID"};
    const std::vector<std::string> T = {"Object", "Boolean", "Byte", "Char", "Short", "Int", "Long",
                                        "Float", "Double", "Void"};
    auto family = [&](const std::string& pre) {
        for (auto& t : T) for (auto s : {"Method", "MethodV", "MethodA"}) n.push_back("Call" + pre + t + s);
    };
    auto fields = [&](const std::string& pre) {
        for (size_t i = 0; i < 9; i++) n.push_back(pre + T[i] + "Field");
    };
    family(""); family("Nonvirtual");
    n.push_back("GetFieldID"); fields("Get"); fields("Set");
    n.push_back("GetStaticMethodID"); family("Static");
    n.push_back("GetStaticFieldID"); fields("GetStatic"); fields("SetStatic");
    for (auto s : {"NewString", "GetStringLength", "GetStringChars", "ReleaseStringChars", "NewStringUTF",
                   "GetStringUTFLength", "GetStringUTFChars", "ReleaseStringUTFChars", "GetArrayLength",
                   "NewObjectArray", "GetObjectArrayElement", "SetObjectArrayElement"}) n.push_back(s);
    auto prim = [&](const std::string& pre, const std::string& suf) {
        for (size_t i = 1; i < 9; i++) n.push_back(pre + T[i] + suf);
    };
    prim("New", "Array"); prim("Get", "ArrayElements"); prim("Release", "ArrayElements");
    prim("Get", "ArrayRegion"); prim("Set", "ArrayRegion");
    for (auto s : {"RegisterNatives", "UnregisterNatives", "MonitorEnter", "MonitorExit", "GetJavaVM",
                   "GetStringRegion", "GetStringUTFRegion", "GetPrimitiveArrayCritical",
                   "ReleasePrimitiveArrayCritical", "GetStringCritical", "ReleaseStringCritical",
                   "NewWeakGlobalRef", "DeleteWeakGlobalRef", "ExceptionCheck", "NewDirectByteBuffer",
                   "GetDirectBufferAddress", "GetDirectBufferCapacity", "GetObjectRefType"}) n.push_back(s);
    return n;
}

struct Emu;
using Handler = std::function<void(Emu&)>;

struct Emu {
    uc_engine* uc = nullptr;
    std::vector<u8> file;
    std::map<u32, std::string> stubs;
    std::unordered_map<std::string, u32> stubByName, dataByName;
    std::unordered_map<std::string, Handler> handlers;
    std::map<std::string, u32> exports;
    std::map<std::string, int> unhandled;
    u32 nextStub = STUB_BASE + 0x10, nextData = DATA_BASE;
    u32 heapPtr = HEAP_BASE + 0x1000, mmapPtr = MMAP_BASE, brk = SBRK_BASE;
    std::unordered_map<u32, u32> blockClass;
    std::map<u32, std::vector<u32>> freeLists;
    std::unordered_map<u32, std::vector<std::string>> handles;
    u32 nextHandle = 0x1000, nextKey = 1, env = 0, vm = 0, libEnd = 0;
    struct Native { std::string cls, name, sig; u32 fn; };
    std::vector<Native> natives;
    std::unordered_map<u32, u32> tls;
    std::vector<std::array<u32, 16>> resumeStack;
    std::vector<std::string> initOrder;

    // ---- guest memory / register helpers
    void rd(u32 a, void* out, size_t n) { if (uc_mem_read(uc, a, out, n)) fatal("mem read", a); }
    void wr(u32 a, const void* p, size_t n) { if (n && uc_mem_write(uc, a, p, n)) fatal("mem write", a); }
    u32 r32(u32 a) { u32 v; rd(a, &v, 4); return v; }
    void w32(u32 a, u32 v) { wr(a, &v, 4); }
    std::string cstr(u32 a, size_t limit = 4096) {
        std::string s;
        while (s.size() < limit) {
            char buf[256];
            size_t chunk = 256 - (a + s.size()) % 256;           // never cross a 256-byte boundary
            rd(a + (u32)s.size(), buf, chunk);
            for (size_t i = 0; i < chunk; i++) { if (!buf[i]) return s; s.push_back(buf[i]); }
        }
        return s;
    }
    u32 reg(int i) { u32 v = 0; uc_reg_read(uc, REGS[i], &v); return v; }
    void setReg(int i, u32 v) { uc_reg_write(uc, REGS[i], &v); }
    u32 arg(int i) { return i < 4 ? reg(i) : r32(reg(SP) + 4 * (i - 4)); }
    void ret(u32 v) { setReg(R0, v); }
    [[noreturn]] void fatal(const char* what, u32 addr) {
        fprintf(stderr, "fatal: %s at %#x (pc=%#x lr=%#x)\n", what, addr, reg(PC), reg(LR)); exit(2);
    }

    // ---- setup
    Emu() {
        if (uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc)) { fprintf(stderr, "uc_open failed\n"); exit(2); }
        u32 cpacr = 0; uc_reg_read(uc, UC_ARM_REG_C1_C0_2, &cpacr); cpacr |= 0xFu << 20;
        uc_reg_write(uc, UC_ARM_REG_C1_C0_2, &cpacr);          // enable VFP (library is VFPv2 softfp)
        u32 fpexc = 0x40000000; uc_reg_write(uc, UC_ARM_REG_FPEXC, &fpexc);
        mapRegion(HEAP_BASE, HEAP_SIZE); mapRegion(MMAP_BASE, MAP_SPAN); mapRegion(SBRK_BASE, MAP_SPAN);
        mapRegion(DATA_BASE, DATA_SIZE); mapRegion(JNI_BASE, JNI_SIZE); mapRegion(TEST_BASE, TEST_SIZE);
        mapRegion(STUB_BASE, STUB_SIZE); mapRegion(STACK_TOP - STACK_SIZE, STACK_SIZE);
        std::vector<u32> bxlr(STUB_SIZE / 4, 0xE12FFF1E);
        wr(STUB_BASE, bxlr.data(), STUB_SIZE);
        uc_hook h;
        uc_hook_add(uc, &h, UC_HOOK_CODE, (void*)stubHook, this, STUB_BASE, STUB_BASE + STUB_SIZE - 1);
        uc_hook_add(uc, &h, UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED,
                    (void*)unmappedHook, this, 1, 0);
        buildJni();
    }
    void mapRegion(u32 a, u32 n) {
        if (uc_mem_map(uc, a, n, UC_PROT_ALL)) { fprintf(stderr, "map %#x+%#x failed\n", a, n); exit(2); }
    }
    static bool unmappedHook(uc_engine*, uc_mem_type, uint64_t addr, int size, int64_t, void* ud) {
        Emu& e = *(Emu*)ud;
        fprintf(stderr, "!! unmapped access addr=%#llx size=%d pc=%#x lr=%#x\n", (unsigned long long)addr, size,
                e.reg(PC), e.reg(LR));
        return false;
    }
    static void stubHook(uc_engine*, uint64_t addr, uint32_t, void* ud) {
        Emu& e = *(Emu*)ud;
        if (addr == EXIT_STUB) return;                          // emu_start(until=EXIT_STUB) stops us
        if (addr == RESUME_STUB) {
            auto regs = e.resumeStack.back(); e.resumeStack.pop_back();
            for (int i = 0; i < 16; i++) e.setReg(i, regs[i]);
            return;
        }
        auto it = e.stubs.find((u32)addr);
        if (it == e.stubs.end()) return;
        auto h = e.handlers.find(it->second);
        if (h == e.handlers.end()) {
            if (e.unhandled[it->second]++ == 0) printf("   [unhandled import] %s\n", it->second.c_str());
            e.ret(0);
        } else h->second(e);
    }

    u32 stubFor(const std::string& name) {
        auto it = stubByName.find(name);
        if (it != stubByName.end()) return it->second;
        u32 a = nextStub; nextStub += 4;
        stubByName[name] = a; stubs[a] = name;
        return a;
    }
    u32 dataSlot(const std::string& name, u32 size) {
        auto it = dataByName.find(name);
        if (it != dataByName.end()) return it->second;
        u32 a = nextData; nextData += (size + 15) & ~15u;
        dataByName[name] = a;
        if (name == "__stack_chk_guard") w32(a, 0xA5A5C0DE);
        if (name == "__page_size") w32(a, 4096);
        return a;
    }
    u32 resolveImport(const std::string& name) {
        static const std::map<std::string, u32> data = {{"__sF", 3 * 88}, {"__stack_chk_guard", 4},
            {"_ctype_", 273}, {"_tolower_tab_", 1044}, {"_toupper_tab_", 1044}, {"__page_size", 4},
            {"__dso_handle", 4}, {"tzname", 8}};
        auto d = data.find(name);
        if (d != data.end()) return dataSlot(name, d->second);
        if (!name.compare(0, 4, "_ZTV") || !name.compare(0, 4, "_ZTI") || !name.compare(0, 4, "_ZTS"))
            return dataSlot(name, 0x100);
        return stubFor(name);
    }
    void buildJni() {
        u32 ftab = JNI_BASE + 0x100, vtab = JNI_BASE + 0x800;
        env = JNI_BASE; vm = JNI_BASE + 0x10;
        w32(env, ftab); w32(vm, vtab);
        auto names = jniNames();
        for (size_t i = 0; i < names.size(); i++) w32(ftab + 4 * (u32)i, stubFor("JNI:" + names[i]));
        const char* jvm[] = {"reserved0", "reserved1", "reserved2", "DestroyJavaVM", "AttachCurrentThread",
                             "DetachCurrentThread", "GetEnv", "AttachCurrentThreadAsDaemon"};
        for (int i = 0; i < 8; i++) w32(vtab + 4 * i, stubFor(std::string("JVM:") + jvm[i]));
    }

    // ---- ELF loading
    bool load(const char* path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
        file.assign(std::istreambuf_iterator<char>(f), {});
        if (file.size() < sizeof(Ehdr) || memcmp(file.data(), "\x7f" "ELF", 4)) { fprintf(stderr, "not ELF\n"); return false; }
        Ehdr eh; memcpy(&eh, file.data(), sizeof eh);
        if (eh.machine != 40) { fprintf(stderr, "not an ARM ELF\n"); return false; }
        std::vector<Phdr> ph(eh.phnum);
        memcpy(ph.data(), file.data() + eh.phoff, eh.phnum * sizeof(Phdr));
        u32 top = 0; const Phdr* dynp = nullptr;
        for (auto& p : ph) {
            if (p.type == PT_LOAD && p.vaddr + p.memsz > top) top = p.vaddr + p.memsz;
            if (p.type == PT_DYNAMIC) dynp = &p;
        }
        top = (top + 0xFFF) & ~0xFFFu; libEnd = LIB_BASE + top;
        mapRegion(LIB_BASE, top);
        for (auto& p : ph) if (p.type == PT_LOAD) wr(LIB_BASE + p.vaddr, file.data() + p.offset, p.filesz);
        if (!dynp) { fprintf(stderr, "no PT_DYNAMIC\n"); return false; }

        std::map<int32_t, u32> dyn;
        for (u32 i = 0; i < dynp->filesz / sizeof(Dyn); i++) {
            Dyn d; memcpy(&d, file.data() + dynp->offset + i * sizeof(Dyn), sizeof d);
            if (d.tag == 0) break;
            dyn[d.tag] = d.val;
        }
        u32 symtab = LIB_BASE + dyn[DT_SYMTAB], strtab = LIB_BASE + dyn[DT_STRTAB];
        u32 nsyms = r32(LIB_BASE + dyn[DT_HASH] + 4);          // nchain == number of dynsym entries
        auto sym = [&](u32 i) { Sym s; rd(symtab + i * sizeof(Sym), &s, sizeof s); return s; };
        for (u32 i = 0; i < nsyms; i++) {
            Sym s = sym(i);
            if (s.shndx) { std::string n = cstr(strtab + s.name); if (!n.empty()) exports[n] = LIB_BASE + s.value; }
        }
        auto relocate = [&](u32 tagTab, u32 tagSz) {
            if (!dyn.count(tagTab)) return;
            u32 tab = LIB_BASE + dyn[tagTab], n = dyn[tagSz] / sizeof(Rel);
            for (u32 k = 0; k < n; k++) {
                Rel r; rd(tab + k * sizeof(Rel), &r, sizeof r);
                u32 type = r.info & 0xff, si = r.info >> 8, off = LIB_BASE + r.offset;
                if (type == R_ARM_RELATIVE) { w32(off, r32(off) + LIB_BASE); continue; }
                Sym s = sym(si);
                u32 S = s.shndx ? LIB_BASE + s.value : resolveImport(cstr(strtab + s.name));
                if (type == R_ARM_GLOB_DAT || type == R_ARM_JUMP_SLOT) w32(off, S);
                else if (type == R_ARM_ABS32) w32(off, S + r32(off));
                else printf("!! unhandled reloc type %u for %s\n", type, cstr(strtab + s.name).c_str());
            }
        };
        relocate(DT_REL, DT_RELSZ); relocate(DT_JMPREL, DT_PLTRELSZ);
        // constructors, in dynamic-loader order: DT_INIT first, then DT_INIT_ARRAY
        if (dyn.count(DT_INIT) && dyn[DT_INIT]) initFns.push_back(LIB_BASE + dyn[DT_INIT]);
        if (dyn.count(DT_INIT_ARRAY))
            for (u32 i = 0; i < dyn[DT_INIT_ARRAYSZ] / 4; i++) {
                u32 fn = r32(LIB_BASE + dyn[DT_INIT_ARRAY] + 4 * i);
                if (fn && fn != 0xffffffffu) initFns.push_back(fn);
            }
        return true;
    }
    std::vector<u32> initFns;

    // ---- running guest code
    bool call(u32 addr, std::initializer_list<u32> args, u32* result = nullptr) {
        int i = 0; for (u32 a : args) setReg(i++, a);
        setReg(SP, STACK_TOP - 0x100); setReg(LR, EXIT_STUB);
        uc_err err = uc_emu_start(uc, addr, EXIT_STUB, 0, 0);
        if (err) { printf("!! guest call %#x failed: %s (pc=%#x)\n", addr, uc_strerror(err), reg(PC)); return false; }
        if (result) *result = reg(R0);
        return true;
    }
    // Run guest function `fn` right now from inside a stub handler, then resume the interrupted
    // caller with r0 = resultAfter.
    void callFromHook(u32 fn, u32 resultAfter) {
        std::array<u32, 16> saved; for (int i = 0; i < 16; i++) saved[i] = reg(i);
        saved[R0] = resultAfter; saved[PC] = saved[LR];
        resumeStack.push_back(saved);
        setReg(LR, RESUME_STUB); setReg(PC, fn);
    }

    // ---- guest heap (size-class allocator; fresh memory is zero, recycled memory is not)
    u32 alloc(u32 n) {
        u32 c = 16; while (c < n) c <<= 1;
        auto& fl = freeLists[c]; u32 a;
        if (!fl.empty()) { a = fl.back(); fl.pop_back(); }
        else {
            if (heapPtr + c > HEAP_BASE + HEAP_SIZE) { fprintf(stderr, "guest heap exhausted\n"); exit(2); }
            a = heapPtr; heapPtr += c;
        }
        blockClass[a] = c; return a;
    }
    void release(u32 p) {
        auto it = blockClass.find(p);
        if (it == blockClass.end()) return;
        freeLists[it->second].push_back(p); blockClass.erase(it);
    }
    u32 newHandle(std::vector<std::string> d) { u32 h = nextHandle; nextHandle += 8; handles[h] = std::move(d); return h; }
    std::string handleStr(u32 h, size_t i, const char* dflt = "?") {
        auto it = handles.find(h); return it != handles.end() && it->second.size() > i ? it->second[i] : dflt;
    }
};

// ------------------------------------------------------------------ HLE library
static void installHle(Emu& E) {
    auto& h = E.handlers;
    auto zero = [](Emu& e) { e.ret(0); };
    // memory
    h["malloc"] = [](Emu& e) { e.ret(e.alloc(e.arg(0))); };
    h["calloc"] = [](Emu& e) {
        u32 n = e.arg(0) * e.arg(1), p = e.alloc(n); std::vector<u8> z(n, 0); e.wr(p, z.data(), n); e.ret(p);
    };
    h["free"] = [](Emu& e) { e.release(e.arg(0)); };
    h["realloc"] = [](Emu& e) {
        u32 p = e.arg(0), n = e.arg(1);
        if (!p) { e.ret(e.alloc(n)); return; }
        auto it = e.blockClass.find(p);
        u32 old = it != e.blockClass.end() ? it->second : n;
        if (n <= old) { e.ret(p); return; }
        u32 q = e.alloc(n); std::vector<u8> tmp(old); e.rd(p, tmp.data(), old); e.wr(q, tmp.data(), old);
        e.release(p); e.ret(q);
    };
    for (auto n : {"_Znwj", "_Znaj", "_ZnwjRKSt9nothrow_t", "_ZnajRKSt9nothrow_t"})
        h[n] = [](Emu& e) { e.ret(e.alloc(e.arg(0))); };
    for (auto n : {"_ZdlPv", "_ZdaPv", "_ZdlPvRKSt9nothrow_t", "_ZdaPvRKSt9nothrow_t"})
        h[n] = [](Emu& e) { e.release(e.arg(0)); };
    Handler memcpyH = [](Emu& e) {
        u32 n = e.arg(2);
        if (n) { std::vector<u8> t(n); e.rd(e.arg(1), t.data(), n); e.wr(e.arg(0), t.data(), n); }
        e.ret(e.arg(0));
    };
    for (auto n : {"memcpy", "memmove", "__aeabi_memcpy", "__aeabi_memcpy4", "__aeabi_memcpy8",
                   "__aeabi_memmove", "__aeabi_memmove4", "__aeabi_memmove8"}) h[n] = memcpyH;
    h["memset"] = [](Emu& e) {
        u32 n = e.arg(2); std::vector<u8> t(n, (u8)e.arg(1)); e.wr(e.arg(0), t.data(), n); e.ret(e.arg(0));
    };
    Handler msetA = [](Emu& e) { u32 n = e.arg(1); std::vector<u8> t(n, (u8)e.arg(2)); e.wr(e.arg(0), t.data(), n); };
    Handler mclrA = [](Emu& e) { u32 n = e.arg(1); std::vector<u8> t(n, 0); e.wr(e.arg(0), t.data(), n); };
    for (auto n : {"__aeabi_memset", "__aeabi_memset4", "__aeabi_memset8"}) h[n] = msetA;
    for (auto n : {"__aeabi_memclr", "__aeabi_memclr4", "__aeabi_memclr8"}) h[n] = mclrA;
    h["memcmp"] = [](Emu& e) {
        u32 n = e.arg(2); std::vector<u8> a(n), b(n); e.rd(e.arg(0), a.data(), n); e.rd(e.arg(1), b.data(), n);
        int c = n ? memcmp(a.data(), b.data(), n) : 0; e.ret(c < 0 ? -1 : c > 0);
    };
    // strings
    h["strlen"] = [](Emu& e) { e.ret((u32)e.cstr(e.arg(0), 1 << 24).size()); };
    h["strcmp"] = [](Emu& e) { int c = e.cstr(e.arg(0), 1 << 24).compare(e.cstr(e.arg(1), 1 << 24)); e.ret(c < 0 ? -1 : c > 0); };
    h["strncmp"] = [](Emu& e) {
        u32 n = e.arg(2); int c = e.cstr(e.arg(0), n).compare(0, n, e.cstr(e.arg(1), n).substr(0, n));
        e.ret(c < 0 ? -1 : c > 0);
    };
    h["strcpy"] = [](Emu& e) { std::string s = e.cstr(e.arg(1), 1 << 24); e.wr(e.arg(0), s.c_str(), s.size() + 1); e.ret(e.arg(0)); };
    h["strncpy"] = [](Emu& e) {
        u32 n = e.arg(2); std::string s = e.cstr(e.arg(1), n).substr(0, n); s.resize(n, '\0');
        e.wr(e.arg(0), s.data(), n); e.ret(e.arg(0));
    };
    h["strcat"] = [](Emu& e) {
        u32 d = e.arg(0); size_t l = e.cstr(d, 1 << 24).size(); std::string s = e.cstr(e.arg(1), 1 << 24);
        e.wr(d + (u32)l, s.c_str(), s.size() + 1); e.ret(d);
    };
    h["strchr"] = [](Emu& e) {
        std::string s = e.cstr(e.arg(0), 1 << 24); char c = (char)e.arg(1); size_t i = c ? s.find(c) : s.size();
        e.ret(i == std::string::npos ? 0 : e.arg(0) + (u32)i);
    };
    h["strrchr"] = [](Emu& e) {
        std::string s = e.cstr(e.arg(0), 1 << 24); size_t i = s.rfind((char)e.arg(1));
        e.ret(i == std::string::npos ? 0 : e.arg(0) + (u32)i);
    };
    h["strstr"] = [](Emu& e) {
        std::string s = e.cstr(e.arg(0), 1 << 24); size_t i = s.find(e.cstr(e.arg(1)));
        e.ret(i == std::string::npos ? 0 : e.arg(0) + (u32)i);
    };
    h["strdup"] = [](Emu& e) {
        std::string s = e.cstr(e.arg(0), 1 << 24); u32 p = e.alloc((u32)s.size() + 1); e.wr(p, s.c_str(), s.size() + 1); e.ret(p);
    };
    // C++ runtime
    h["__cxa_atexit"] = h["__aeabi_atexit"] = zero;
    h["__cxa_guard_acquire"] = [](Emu& e) { u8 b; e.rd(e.arg(0), &b, 1); e.ret(b & 1 ? 0 : 1); };
    h["__cxa_guard_release"] = [](Emu& e) { u8 one = 1; e.wr(e.arg(0), &one, 1); };
    h["__cxa_guard_abort"] = [](Emu&) {};
    h["__stack_chk_fail"] = [](Emu& e) { printf("!! __stack_chk_fail\n"); uc_emu_stop(e.uc); };
    h["abort"] = [](Emu& e) { printf("!! abort() from lr=%#x\n", e.reg(LR)); uc_emu_stop(e.uc); };
    // threads: single-threaded for now, every lock succeeds immediately
    for (auto n : {"pthread_mutex_init", "pthread_mutex_lock", "pthread_mutex_unlock", "pthread_mutex_destroy",
                   "pthread_mutex_trylock", "pthread_mutexattr_init", "pthread_mutexattr_destroy",
                   "pthread_mutexattr_settype", "pthread_mutexattr_setpshared", "pthread_cond_init",
                   "pthread_cond_signal", "pthread_cond_broadcast", "pthread_cond_destroy", "pthread_attr_init",
                   "pthread_attr_destroy", "pthread_attr_setstacksize", "pthread_attr_setdetachstate", "sem_init",
                   "sem_post", "sem_destroy", "sched_yield"}) h[n] = zero;
    h["pthread_key_create"] = [](Emu& e) { e.w32(e.arg(0), e.nextKey++); e.ret(0); };
    h["pthread_getspecific"] = [](Emu& e) { auto it = e.tls.find(e.arg(0)); e.ret(it == e.tls.end() ? 0 : it->second); };
    h["pthread_setspecific"] = [](Emu& e) { e.tls[e.arg(0)] = e.arg(1); e.ret(0); };
    h["pthread_self"] = [](Emu& e) { e.ret(1); };
    h["pthread_once"] = [](Emu& e) {
        u32 ctl = e.arg(0), fn = e.arg(1);
        if (e.r32(ctl) == 0) { e.w32(ctl, 1); e.callFromHook(fn, 0); } else e.ret(0);
    };
    // bionic atomics
    h["__atomic_inc"] = [](Emu& e) { u32 p = e.arg(0), v = e.r32(p); e.w32(p, v + 1); e.ret(v); };
    h["__atomic_dec"] = [](Emu& e) { u32 p = e.arg(0), v = e.r32(p); e.w32(p, v - 1); e.ret(v); };
    h["__atomic_swap"] = [](Emu& e) { u32 p = e.arg(1), v = e.r32(p); e.w32(p, e.arg(0)); e.ret(v); };
    h["__atomic_cmpxchg"] = [](Emu& e) {
        u32 p = e.arg(2), v = e.r32(p);
        if (v == e.arg(0)) { e.w32(p, e.arg(1)); e.ret(0); } else e.ret(1);
    };
    // address space
    h["mmap"] = [](Emu& e) {
        u32 len = (e.arg(1) + 0xFFF) & ~0xFFFu, flags = e.arg(3), fd = e.arg(4);
        if (!(flags & 0x20) && fd != 0xFFFFFFFFu) { printf("   [mmap file-backed fd=%d]\n", (int)fd); e.ret(0xFFFFFFFFu); return; }
        u32 a = e.mmapPtr; e.mmapPtr += len; e.ret(a);
    };
    h["munmap"] = zero;
    h["sbrk"] = [](Emu& e) { u32 old = e.brk; e.brk += e.arg(0); e.ret(old); };
    // misc libc
    h["__errno"] = [](Emu& e) { e.ret(e.dataSlot("errno_slot", 4)); };
    h["sysconf"] = [](Emu& e) { e.ret(e.arg(0) == 39 ? 4096 : 1); };
    h["getenv"] = zero;
    h["time"] = [](Emu& e) { e.ret(1700000000); };
    h["clock"] = [](Emu& e) {
        using namespace std::chrono;
        e.ret((u32)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
    };
    h["clock_gettime"] = [](Emu& e) {
        using namespace std::chrono;
        auto us = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        e.w32(e.arg(1), (u32)(us / 1000000)); e.w32(e.arg(1) + 4, (u32)(us % 1000000) * 1000); e.ret(0);
    };
    h["fopen"] = [](Emu& e) { printf("   [fopen] %s (%s) -> NULL\n", e.cstr(e.arg(0)).c_str(), e.cstr(e.arg(1)).c_str()); e.ret(0); };
    h["__android_log_print"] = [](Emu& e) { printf("   [android_log] %s\n", e.cstr(e.arg(2)).c_str()); };
    h["__android_log_write"] = [](Emu& e) { printf("   [android_log] %s | %s\n", e.cstr(e.arg(2)).c_str(), e.cstr(e.arg(3)).c_str()); };

    // JNI: enough for JNI_OnLoad
    h["JVM:GetEnv"] = [](Emu& e) { e.w32(e.arg(1), e.env); e.ret(0); };
    h["JNI:GetVersion"] = [](Emu& e) { e.ret(0x10006); };
    h["JNI:FindClass"] = [](Emu& e) { e.ret(e.newHandle({"class", e.cstr(e.arg(1))})); };
    for (auto n : {"NewGlobalRef", "NewLocalRef", "NewWeakGlobalRef"}) h[std::string("JNI:") + n] = [](Emu& e) { e.ret(e.arg(1)); };
    for (auto n : {"DeleteLocalRef", "DeleteGlobalRef", "ExceptionClear", "DeleteWeakGlobalRef"}) h[std::string("JNI:") + n] = [](Emu&) {};
    h["JNI:ExceptionCheck"] = h["JNI:ExceptionOccurred"] = zero;
    auto mid = [](const char* kind) {
        return [kind](Emu& e) {
            std::string cls = e.handleStr(e.arg(1), 1), nm = e.cstr(e.arg(2)), sg = e.cstr(e.arg(3));
            printf("   JNI %-8s %s.%s %s\n", kind, cls.c_str(), nm.c_str(), sg.c_str());
            e.ret(e.newHandle({kind, cls, nm, sg}));
        };
    };
    h["JNI:GetStaticMethodID"] = mid("static"); h["JNI:GetMethodID"] = mid("method");
    h["JNI:GetFieldID"] = mid("field"); h["JNI:GetStaticFieldID"] = mid("sfield");
    h["JNI:RegisterNatives"] = [](Emu& e) {
        std::string cls = e.handleStr(e.arg(1), 1); u32 arr = e.arg(2), n = e.arg(3);
        for (u32 i = 0; i < n; i++)
            e.natives.push_back({cls, e.cstr(e.r32(arr + 12 * i)), e.cstr(e.r32(arr + 12 * i + 4)), e.r32(arr + 12 * i + 8)});
        e.ret(0);
    };
}

// pthread_once must be able to run a guest function from inside a host handler and resume the
// caller afterwards. This tiny hand-assembled ARM routine checks exactly that.
static bool selfTest(Emu& e) {
    const u32 code[] = {0xe92d4010, 0xe59f0014, 0xe59f1014, 0xe59f2014, 0xe12fff32, 0xe8bd8010, 0xe1a00000,
                        0xe1a00000,
                        TEST_BASE + 0x1000,                    // 0x20: pthread_once control word
                        TEST_BASE + 0x30,                      // 0x24: init routine
                        e.stubFor("pthread_once"),             // 0x28
                        0,
                        0xe3a0002a, 0xe59f1004, 0xe5810000, 0xe12fff1e,   // 0x30: marker = 42
                        TEST_BASE + 0x1004};                   // 0x40: marker address
    e.wr(TEST_BASE, code, sizeof code);
    e.w32(TEST_BASE + 0x1000, 0); e.w32(TEST_BASE + 0x1004, 0);
    u32 r = 99;
    bool ok = e.call(TEST_BASE, {}, &r) && e.r32(TEST_BASE + 0x1004) == 42 && r == 0;
    e.w32(TEST_BASE + 0x1004, 0);
    ok = ok && e.call(TEST_BASE, {}, &r) && e.r32(TEST_BASE + 0x1004) == 0 && r == 0;   // second call: no re-run
    printf("self-test (host->guest call from a stub, resume): %s\n", ok ? "OK" : "FAILED");
    return ok;
}

int main(int argc, char** argv) {
    const char* path = "apk/lib/armeabi/libDeadSpace.so";
    bool selftest = false;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--selftest")) selftest = true; else path = argv[i]; }

    Emu E;
    installHle(E);
    if (selftest && !selfTest(E)) return 1;
    if (!E.load(path)) return 1;

    size_t imports = 0; for (auto& s : E.stubs) if (s.second.compare(0, 4, "JNI:") && s.second.compare(0, 4, "JVM:")) imports++;
    printf("loaded %s: base=%#x end=%#x, %zu import stubs, %zu exports\n", path, LIB_BASE, E.libEnd, imports, E.exports.size());

    printf("running %zu initializers\n", E.initFns.size());
    size_t ran = 0;
    for (u32 fn : E.initFns) { if (!E.call(fn, {})) break; ran++; }
    printf("%zu/%zu initializers completed\n", ran, E.initFns.size());

    auto onload = E.exports.find("JNI_OnLoad");
    if (onload == E.exports.end()) { printf("no JNI_OnLoad\n"); return 1; }
    u32 v = 0;
    printf("calling JNI_OnLoad @ %#x\n", onload->second);
    if (E.call(onload->second, {E.vm, 0}, &v)) printf("JNI_OnLoad returned %#x\n", v);

    printf("\nregistered natives (RegisterNatives): %zu\n", E.natives.size());
    size_t java = 0; for (auto& x : E.exports) if (!x.first.compare(0, 5, "Java_")) java++;
    printf("exported Java_* entry points: %zu\n", java);
    if (!E.unhandled.empty()) {
        printf("\nimports hit but not implemented yet:");
        for (auto& u : E.unhandled) printf(" %s(x%d)", u.first.c_str(), u.second);
        printf("\n");
    }
    return 0;
}
