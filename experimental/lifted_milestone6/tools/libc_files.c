// libc_files.c - files, stdio, stat and time for the translated game (included by host.c, not in COMPARE_MODE).
// Structure layouts follow the Android (bionic, ARM) ABI that the game was compiled against.
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
/* Runtime-only resource audit. Missing files are reported automatically,
 * successes only with DS_ASSET_TRACE=1. Avoid flooding normal play logs. */
static int ds_asset_io_verbose(void) {
    static int cached = -1;
    if (cached < 0) {
        const char* val = getenv("DS_ASSET_TRACE");
        cached = val && *val && strcmp(val, "0") != 0;
    }
    return cached;
}
static void ds_asset_io_note(const char* op, const char* path, int ok, int err) {
    static unsigned missing_count;
    if (ok && !ds_asset_io_verbose()) return;
    if (!ok && missing_count++ >= 96 && !ds_asset_io_verbose()) return;
    fprintf(stderr, "[GUEST_RESOURCE_%s] op=%s path=%s errno=%d\n",
            ok ? "OK" : "MISS", op, path ? path : "(null)", ok ? 0 : err);
}

static FILE* hostfile(u32 f) {
    u32 sf = data_object("__sF");
    if (f >= sf && f < sf + 3 * 84) { u32 i = (f - sf) / 84; return i == 0 ? stdin : i == 1 ? stdout : stderr; }
    return (FILE*)P(f);
}
static void fill_stat(u32 buf, const struct stat* s) {
    u8* b = P(buf); memset(b, 0, 104);
    *(u64*)(b + 0) = (u64)s->st_dev; *(u32*)(b + 12) = (u32)s->st_ino; *(u32*)(b + 16) = (u32)s->st_mode; *(u32*)(b + 20) = (u32)s->st_nlink;
    *(u32*)(b + 24) = (u32)s->st_uid; *(u32*)(b + 28) = (u32)s->st_gid; *(u64*)(b + 48) = (u64)s->st_size;
#ifdef _WIN32
    *(u32*)(b + 56) = 4096u;
#else
    *(u32*)(b + 56) = (u32)s->st_blksize;
#endif
    #ifdef _WIN32
    *(u64*)(b + 64) = ((u64)s->st_size + 511u) / 512u;
#else
    *(u64*)(b + 64) = (u64)s->st_blocks;
#endif
    *(u32*)(b + 72) = (u32)s->st_atime;
    *(u32*)(b + 80) = (u32)s->st_mtime;
    *(u32*)(b + 88) = (u32)s->st_ctime;
    *(u64*)(b + 96) = (u64)s->st_ino;
}
IMP(stat)  { T(stat); const char* path = P(r0); struct stat s; int rc = stat(path, &s); int saved = errno;
    if (!rc) fill_stat(r1, &s); else g_errno = saved;
    ds_asset_io_note("stat", path, rc == 0, saved); errno = saved; RET(rc); }
IMP(fstat) { T(fstat); struct stat s; int rc = fstat((int)r0, &s); int saved = errno;
    if (!rc) fill_stat(r1, &s); else g_errno = saved; errno = saved; RET(rc); }
/* Android Bionic ARM32 <fcntl.h> flags differ from MSVCRT/MinGW values.
 * Previous Win32 code forwarded raw Android bits, causing O_CREAT (0x40)
 * and O_APPEND (0x400) to be treated as unrelated Windows flags. In
 * particular, first-run saves could never be created by guest open().
 * Keep this adapter restricted to Win32; Linux already uses matching flags.
 */
#ifdef _WIN32
#define DS_GUEST_O_CREAT   0x0040u
#define DS_GUEST_O_EXCL    0x0080u
#define DS_GUEST_O_TRUNC   0x0200u
#define DS_GUEST_O_APPEND  0x0400u
static int ds_win32_from_guest_openflags(u32 f) {
    int result;
    switch (f & 3u) {
      case 0: result = O_RDONLY; break;
      case 1: result = O_WRONLY; break;
      case 2: result = O_RDWR; break;
      default: return -1;
    }
    if (f & DS_GUEST_O_CREAT) result |= O_CREAT;
    if (f & DS_GUEST_O_EXCL) result |= O_EXCL;
    if (f & DS_GUEST_O_TRUNC) result |= O_TRUNC;
    if (f & DS_GUEST_O_APPEND) result |= O_APPEND;
    return result | O_BINARY;
}
#endif
#ifdef _WIN32
/* Android exposes /proc/cpuinfo as a readable text file. Win32 doesn't.
 * Supply a conservative ARMv7 device profile for its legacy feature probe.
 * Do not depend on host CPU features: the lifted game is native x86 now. */
static FILE* ds_win32_cpuinfo(void) {
    FILE* f = tmpfile();
    if (!f) return NULL;
    static const char info[] =
        "Processor\t: ARMv7 Processor rev 2 (v7l)\n"
        "BogoMIPS\t: 1000.00\n"
        "Features\t: swp half thumb fastmult vfp edsp tls\n"
        "CPU architecture: 7\n"
        "Hardware\t: Qualcomm MSM8255\n";
    fwrite(info, 1, sizeof info - 1, f);
    rewind(f);
    return f;
}
#endif
IMP(open)  { T(open); const char* path = P(r0);
#ifdef _WIN32
    int flags = ds_win32_from_guest_openflags(r1);
    if (path && strcmp(path, "/proc/cpuinfo") == 0 &&
        (r1 & 3u) == 0 && !(r1 & DS_GUEST_O_CREAT)) {
        FILE* ps = ds_win32_cpuinfo();
        if (!ps) { g_errno = errno; RET(-1); }
        int pseudo_fd = dup(fileno(ps));
        int saved_errno = errno;
        fclose(ps);
        if (pseudo_fd < 0) { g_errno = saved_errno; RET(-1); }
        fprintf(stderr, "[GUEST_ANDROID_PROC] /proc/cpuinfo supplied (open)\n");
        RET(pseudo_fd);
    }
    if (flags < 0) {
        g_errno = errno = EINVAL;
        ds_asset_io_note("open_invalid_flags", path, 0, errno);
        RET(-1);
    }
    if (r1 & DS_GUEST_O_CREAT) {
        fprintf(stderr, "[GUEST_SAVE_CREATE] flags=0x%x host_flags=0x%x path=%s\n",
                (unsigned)r1, (unsigned)flags, path ? path : "(null)");
    }
    int fd = open(path, flags, (mode_t)r2);
#else
    int fd = open(path, (int)r1, (mode_t)r2);
#endif
    int saved = errno;
    if (fd < 0) g_errno = saved;
    ds_asset_io_note("open", path, fd >= 0, saved); errno = saved; RET(fd); }
IMP(close) { T(close); int rc = close((int)r0); if (rc < 0) g_errno = errno; RET(rc); }
IMP(read)  { T(read); ssize_t rc = read((int)r0, P(r1), r2); if (rc < 0) g_errno = errno; RET(rc); }
IMP(write) { T(write); ssize_t rc = write((int)r0, P(r1), r2); if (rc < 0) g_errno = errno; RET(rc); }
IMP(lseek) { T(lseek); off_t rc = lseek((int)r0, (off_t)(s32)r1, (int)r2);
    if (rc == (off_t)-1) g_errno = errno; RET((u32)rc); }
IMP(fsync) { T(fsync); int rc = fsync((int)r0); if (rc < 0) g_errno = errno; RET(rc); }
IMP(ftruncate) { T(ftruncate); int rc = ftruncate((int)r0, (off_t)(s32)r1); if (rc < 0) g_errno = errno; RET(rc); }
IMP(unlink) { T(unlink); int rc = unlink(P(r0)); if (rc < 0) g_errno = errno; RET(rc); }
IMP(rmdir)  { T(rmdir); int rc = rmdir(P(r0)); if (rc < 0) g_errno = errno; RET(rc); }
IMP(mkdir)  { T(mkdir); int rc = mkdir(P(r0), (mode_t)r1); if (rc < 0) g_errno = errno; RET(rc); }
IMP(rename) { T(rename); int rc = rename(P(r0), P(r1)); if (rc < 0) g_errno = errno; RET(rc); }
IMP(remove) { T(remove); int rc = remove(P(r0)); if (rc < 0) g_errno = errno; RET(rc); }
IMP(chdir)  { T(chdir); int rc = chdir(P(r0)); if (rc < 0) g_errno = errno; RET(rc); }
IMP(chmod)  { T(chmod); int rc = chmod(P(r0), (mode_t)r1); if (rc < 0) g_errno = errno; RET(rc); }
IMP(getcwd) { T(getcwd); char* rc = getcwd(P(r0), r1); if (!rc) g_errno = errno; RET(U(rc)); }

IMP(fopen)   { T(fopen); const char* path = P(r0);
#ifdef _WIN32
    if (path && strcmp(path, "/proc/cpuinfo") == 0 && r1 &&
        *(char*)P(r1) == 'r') {
        FILE* pseudo = ds_win32_cpuinfo();
        if (!pseudo) { g_errno = errno; RET(0); }
        fprintf(stderr, "[GUEST_ANDROID_PROC] /proc/cpuinfo supplied (fopen)\n");
        RET(U(pseudo));
    }
#endif
    FILE* fp = fopen(path, P(r1)); int saved = errno;
    if (!fp) g_errno = saved;
    ds_asset_io_note("fopen", path, fp != NULL, saved); errno = saved; RET(U(fp)); }
IMP(fclose)  { T(fclose); int rc = fclose(hostfile(r0)); if (rc == EOF) g_errno = errno; RET(rc); }
IMP(fread)   { T(fread);   RET(fread(P(r0), r1, r2, hostfile(r3))); }
IMP(fwrite)  { T(fwrite);  RET(fwrite(P(r0), r1, r2, hostfile(r3))); }
IMP(fseek)   { T(fseek); int rc = fseek(hostfile(r0), (long)(s32)r1, (int)r2); if (rc < 0) g_errno = errno; RET(rc); }
IMP(ftell)   { T(ftell); long rc = ftell(hostfile(r0)); if (rc < 0) g_errno = errno; RET((u32)rc); }
IMP(fgets)   { T(fgets);   RET(U(fgets(P(r0), (int)r1, hostfile(r2)))); }
IMP(fgetc)   { T(fgetc);   RET(fgetc(hostfile(r0))); }
IMP(ungetc)  { T(ungetc); int rc = ungetc((int)r0, hostfile(r1)); if (rc == EOF) g_errno = errno; RET(rc); }
IMP(fflush)  { T(fflush); int rc = r0 ? fflush(hostfile(r0)) : fflush(NULL); if (rc == EOF) g_errno = errno; RET(rc); }
IMP(fputs)   { T(fputs);   RET(fputs(P(r0), hostfile(r1)) < 0 ? -1 : 0); }
IMP(fwide)   { T(fwide);   RET(0); }
IMP(fprintf) {
    T(fprintf); char buf[4096]; u32 regs[4] = {0, 0, r2, r3}; AR a = {regs, 4, 2, (const u32*)P(sp)};
    int n = gformat(buf, sizeof buf, P(r1), &a); fwrite(buf, 1, n < (int)sizeof buf ? n : (int)sizeof buf - 1, hostfile(r0)); RET(n);
}
IMP(opendir)  { T(opendir); const char* path = P(r0); DIR* d = opendir(path); int saved = errno;
    if (!d) g_errno = saved;
    ds_asset_io_note("opendir", path, d != NULL, saved); errno = saved; RET(U(d)); }
IMP(closedir) { T(closedir); int rc = closedir(P(r0)); if (rc < 0) g_errno = errno; RET(rc); }
static u8 g_dirent[280];
IMP(readdir) {                                               // bionic struct dirent: ino(8) off(8) reclen(2) type(1) name[256]
    T(readdir); struct dirent* e = readdir(P(r0)); if (!e) RET(0);
    memset(g_dirent, 0, sizeof g_dirent); *(u64*)(g_dirent + 0) = e->d_ino; *(u16*)(g_dirent + 16) = 280;
#ifdef _WIN32
    g_dirent[18] = 0; /* Win32 dirent does not expose d_type reliably. */
#else
    g_dirent[18] = e->d_type;
#endif
    strncpy((char*)g_dirent + 19, e->d_name, 255); RET(U(g_dirent));
}
IMP(localtime) { T(localtime); time_t t = (time_t)*(s32*)P(r0); RET(U(localtime(&t))); }
IMP(gmtime)    { T(gmtime);    time_t t = (time_t)*(s32*)P(r0); RET(U(gmtime(&t))); }
IMP(mktime)    { T(mktime);    RET((u32)mktime(P(r0))); }
IMP(__android_log_vprint) { T(__android_log_vprint); RET(0); }
IMP(poll)      { T(poll);      RET(0); }
IMP(prctl)     { T(prctl);     RET(0); }
IMP(fcntl)     { T(fcntl);     RET(0); }
IMP(raise)     { T(raise);     fprintf(stderr, "!! guest raise(%d)\n", (int)r0); exit(3); }
IMP(setenv)    { T(setenv);    RET(0); }
IMP(wcslen)    { T(wcslen);    u32 n = 0; while (((u32*)P(r0))[n]) n++; RET(n); }
