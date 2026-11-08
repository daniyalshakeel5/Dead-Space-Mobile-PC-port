// Differential-test harness: runs lifted functions (32-bit x86, guest ptr == host ptr).
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include "rt.h"

#define LIB_BASE 0x10000000u
#define SCRATCH  0x60000000u
#define SCRATCH_SZ 0x10000u
#define STACK_TOP 0x7f000000u
#define STACK_SZ  0x800000u

u32 g_base = LIB_BASE;
__thread u32 ds_arm_vfp[32];
__thread u32 ds_arm_fpscr;
void trap_unsupported(u32 a, const char* w) { fprintf(stderr, "TRAP %x %s\n", a, w); abort(); }
u64 call_ind(u32 t, u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { (void)t;(void)r0;(void)r1;(void)r2;(void)r3;(void)sp; abort(); return 0; }

#include "lifted.c"          /* generated: functions + FN[] table */

static u32 crc_table[256];
static void crc_init(void) { for (u32 i = 0; i < 256; i++) { u32 c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_table[i] = c; } }
static u32 crc32(const u8* p, size_t n) { u32 c = 0xFFFFFFFFu; while (n--) c = crc_table[(c ^ *p++) & 255] ^ (c >> 8); return c ^ 0xFFFFFFFFu; }
static void fill(u32 addr, u32 nbytes, u32 seed) {
    for (u32 i = 0; i < nbytes / 4; i++) { seed = seed * 1664525u + 1013904223u; *(u32*)(uintptr_t)(addr + 4 * i) = seed; }
}
static void fill_f(u32 addr, u32 nbytes, u32 seed) {
    for (u32 i = 0; i < nbytes / 4; i++) {
        seed = seed * 1664525u + 1013904223u;
        float f = (float)((s32)((seed >> 8) & 0xFFFFFFu) - 0x800000) / 1024.0f;
        memcpy((void*)(uintptr_t)(addr + 4 * i), &f, 4);
    }
}
static void* map_fixed(u32 addr, u32 size) {
    void* p = mmap((void*)(uintptr_t)addr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); exit(2); }
    return p;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: harness image.bin vectors.txt\n"); return 2; }
    crc_init();
    FILE* f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long isz = ftell(f); fseek(f, 0, SEEK_SET);
    map_fixed(LIB_BASE, (u32)((isz + 0xFFF) & ~0xFFF));
    if (fread((void*)(uintptr_t)LIB_BASE, 1, isz, f) != (size_t)isz) return 2;
    fclose(f);
    map_fixed(SCRATCH, SCRATCH_SZ);
    map_fixed(STACK_TOP - STACK_SZ, STACK_SZ);

    FILE* v = fopen(argv[2], "r");
    unsigned addr, r0, r1, r2, r3, seed, mode;
    while (fscanf(v, "%x %x %x %x %x %u %u", &addr, &r0, &r1, &r2, &r3, &seed, &mode) == 7) {
        fflush(stdout);
        pid_t pid = fork();
        if (pid == 0) {
            alarm(3);
            fn_t fn = NULL;
            for (size_t i = 0; i < sizeof FN / sizeof FN[0]; i++) if (FN[i].addr == addr) fn = FN[i].fn;
            if (!fn) { printf("N %x\n", addr); fflush(stdout); _exit(0); }
            u32 sp = STACK_TOP - 0x100;
            if (mode) { fill_f(SCRATCH, SCRATCH_SZ, seed); fill_f(sp, 0x100, seed ^ 0x5a5a5a5a); }
            else { fill(SCRATCH, SCRATCH_SZ, seed); fill(sp, 0x100, seed ^ 0x5a5a5a5a); }
            u64 r = fn(r0, r1, r2, r3, sp);
            if (getenv("DUMP")) { FILE* df = fopen(getenv("DUMP"), "wb"); fwrite((void*)(uintptr_t)SCRATCH, 1, SCRATCH_SZ, df); fclose(df); }
            printf("R %x %x %x %x\n", addr, (u32)r, (u32)(r >> 32), crc32((u8*)(uintptr_t)SCRATCH, SCRATCH_SZ));
            fflush(stdout); _exit(0);
        }
        int st; waitpid(pid, &st, 0);
        if (!WIFEXITED(st)) { printf("X %x %d\n", addr, WIFSIGNALED(st) ? WTERMSIG(st) : -1); }
    }
    return 0;
}
