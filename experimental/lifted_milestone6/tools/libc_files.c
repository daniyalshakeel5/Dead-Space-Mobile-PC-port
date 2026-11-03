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
    *(u32*)(b + 24) = (u32)s->st_uid; *(u32*)(b + 28) = (u32)s->st_gid; *(u64*)(b + 48) = (u64)s->st_size; *(u32*)(b + 56) = (u32)s->st_blksize;
    *(u64*)(b + 64) = (u64)s->st_blocks; *(u32*)(b + 72) = (u32)s->st_atime; *(u32*)(b + 80) = (u32)s->st_mtime; *(u32*)(b + 88) = (u32)s->st_ctime;
    *(u64*)(b + 96) = (u64)s->st_ino;
}
IMP(stat)  { T(stat); const char* path = P(r0); struct stat s; int rc = stat(path, &s); int saved = errno;
    if (!rc) fill_stat(r1, &s); ds_asset_io_note("stat", path, rc == 0, saved); errno = saved; RET(rc); }
IMP(fstat) { T(fstat); struct stat s; int rc = fstat((int)r0, &s); if (!rc) fill_stat(r1, &s); RET(rc); }
IMP(open)  { T(open); const char* path = P(r0); int fd = open(path, (int)r1, (mode_t)r2); int saved = errno;
    ds_asset_io_note("open", path, fd >= 0, saved); errno = saved; RET(fd); }
IMP(close) { T(close); RET(close((int)r0)); }
IMP(read)  { T(read);  RET(read((int)r0, P(r1), r2)); }
IMP(write) { T(write); RET(write((int)r0, P(r1), r2)); }
IMP(lseek) { T(lseek); RET((u32)lseek((int)r0, (off_t)(s32)r1, (int)r2)); }
IMP(fsync) { T(fsync); RET(fsync((int)r0)); }
IMP(ftruncate) { T(ftruncate); RET(ftruncate((int)r0, (off_t)(s32)r1)); }
IMP(unlink) { T(unlink); RET(unlink(P(r0))); }
IMP(rmdir)  { T(rmdir);  RET(rmdir(P(r0))); }
IMP(mkdir)  { T(mkdir);  RET(mkdir(P(r0), (mode_t)r1)); }
IMP(rename) { T(rename); RET(rename(P(r0), P(r1))); }
IMP(remove) { T(remove); RET(remove(P(r0))); }
IMP(chdir)  { T(chdir);  RET(chdir(P(r0))); }
IMP(chmod)  { T(chmod);  RET(chmod(P(r0), (mode_t)r1)); }
IMP(getcwd) { T(getcwd); RET(U(getcwd(P(r0), r1))); }

IMP(fopen)   { T(fopen); const char* path = P(r0); FILE* fp = fopen(path, P(r1)); int saved = errno;
    ds_asset_io_note("fopen", path, fp != NULL, saved); errno = saved; RET(U(fp)); }
IMP(fclose)  { T(fclose);  RET(fclose(hostfile(r0))); }
IMP(fread)   { T(fread);   RET(fread(P(r0), r1, r2, hostfile(r3))); }
IMP(fwrite)  { T(fwrite);  RET(fwrite(P(r0), r1, r2, hostfile(r3))); }
IMP(fseek)   { T(fseek);   RET(fseek(hostfile(r0), (long)(s32)r1, (int)r2)); }
IMP(ftell)   { T(ftell);   RET((u32)ftell(hostfile(r0))); }
IMP(fgets)   { T(fgets);   RET(U(fgets(P(r0), (int)r1, hostfile(r2)))); }
IMP(fgetc)   { T(fgetc);   RET(fgetc(hostfile(r0))); }
IMP(ungetc)  { T(ungetc);  RET(ungetc((int)r0, hostfile(r1))); }
IMP(fflush)  { T(fflush);  RET(r0 ? fflush(hostfile(r0)) : fflush(NULL)); }
IMP(fputs)   { T(fputs);   RET(fputs(P(r0), hostfile(r1)) < 0 ? -1 : 0); }
IMP(fwide)   { T(fwide);   RET(0); }
IMP(fprintf) {
    T(fprintf); char buf[4096]; u32 regs[4] = {0, 0, r2, r3}; AR a = {regs, 4, 2, (const u32*)P(sp)};
    int n = gformat(buf, sizeof buf, P(r1), &a); fwrite(buf, 1, n < (int)sizeof buf ? n : (int)sizeof buf - 1, hostfile(r0)); RET(n);
}
IMP(opendir)  { T(opendir); const char* path = P(r0); DIR* d = opendir(path); int saved = errno;
    ds_asset_io_note("opendir", path, d != NULL, saved); errno = saved; RET(U(d)); }
IMP(closedir) { T(closedir); RET(closedir(P(r0))); }
static u8 g_dirent[280];
IMP(readdir) {                                               // bionic struct dirent: ino(8) off(8) reclen(2) type(1) name[256]
    T(readdir); struct dirent* e = readdir(P(r0)); if (!e) RET(0);
    memset(g_dirent, 0, sizeof g_dirent); *(u64*)(g_dirent + 0) = e->d_ino; *(u16*)(g_dirent + 16) = 280; g_dirent[18] = e->d_type;
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
