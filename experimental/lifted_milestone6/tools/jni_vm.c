// jni_vm.c - a tiny fake Java VM for the translated game (included by host.c).
// JNIEnv/JavaVM function tables, object handles, strings, arrays, calls into "Java" methods that the
// game expects from Android (AssetManager, InputStream, delegates ...). Unknown Java methods are logged.
#include <dirent.h>
#include <sys/stat.h>

enum { K_NONE, K_CLASS, K_STRING, K_OBJECT, K_ARRAY, K_OBJARR, K_ASSETMGR, K_STREAM, K_AFD };
typedef struct { u8 kind; u8 esz; char* s; void* p; u32 n; u32 extra; } JObj;
#define OBJ_CHUNK 1024
static JObj* g_chunk[4096]; static u32 g_nobjs; /* published with release/acquire */
static pthread_mutex_t g_objlock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;       // object table, class cache and Java-side state
#define HANDLE(i) (0x10000u + 8u * (i))
static inline JObj* obj_at(u32 i) { return &g_chunk[i / OBJ_CHUNK][i % OBJ_CHUNK]; }
/* Obj slot/chunk is fully initialized before the handle becomes visible.
 * Readers use an acquire-load; g_chunk[] entries are stable once created.
 * Avoid a C data race when texture worker threads read freshly made objects. */
#define OBJ(h) (((h) >= 0x10000u && (((h) - 0x10000u) & 7) == 0 && ((h) - 0x10000u) / 8 < __atomic_load_n(&g_nobjs, __ATOMIC_ACQUIRE)) ? obj_at(((h) - 0x10000u) / 8) : (JObj*)0)

static u32 new_obj(u8 kind, const char* s, void* p, u32 n) {
    pthread_mutex_lock(&g_objlock);
    u32 i = __atomic_load_n(&g_nobjs, __ATOMIC_RELAXED), c = i / OBJ_CHUNK;
    if (c >= 4096) { fprintf(stderr, "!! JNI object table full\n"); exit(3); }
    if (!g_chunk[c]) g_chunk[c] = calloc(OBJ_CHUNK, sizeof(JObj));         // chunks never move, so JObj* stays valid across threads
    JObj* o = &g_chunk[c][i % OBJ_CHUNK]; memset(o, 0, sizeof *o);
    o->kind = kind; o->s = s ? strdup(s) : 0; o->p = p; o->n = n;
    __atomic_store_n(&g_nobjs, i + 1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&g_objlock);
    return HANDLE(i);
}
static const char* g_assets_root = "assets";
static __thread int g_exception;              // JNI exception state is per calling thread (Bionic/JNI)
static u32 g_activity, g_assetmgr;
static int g_log_java = 1;

typedef struct { char cls[96], name[64], sig[160]; int is_static; } Mid;

// ---- class handles are unique per name
static u32 class_handle(const char* name) {
    static struct { char* name; u32 h; } cache[1024]; static int n;
    pthread_mutex_lock(&g_objlock);
    for (int i = 0; i < n; i++) if (!strcmp(cache[i].name, name)) { u32 h = cache[i].h; pthread_mutex_unlock(&g_objlock); return h; }
    u32 h = new_obj(K_CLASS, name, 0, 0);
    if (n < 1024) { cache[n].name = strdup(name); cache[n++].h = h; }
    pthread_mutex_unlock(&g_objlock);
    return h;
}
static const char* cls_of(u32 h) {              // class name for a class handle or an object handle
    JObj* o = OBJ(h); if (!o) return "?";
    if (o->kind == K_CLASS) return o->s;
    switch (o->kind) {
        case K_STRING: return "java/lang/String"; case K_ASSETMGR: return "android/content/res/AssetManager";
        case K_STREAM: return "java/io/InputStream"; case K_AFD: return "android/content/res/AssetFileDescriptor";
        default: return o->s ? o->s : "?";
    }
}

// ---- Java argument handling
typedef union { u32 i; u64 j; float f; double d; } JArg;
static int parse_sig(const char* sig, char* kinds, char* ret) {       // kinds: one char per parameter ('L' for objects/arrays)
    int n = 0; const char* p = sig + 1;
    while (*p != ')') {
        char c = *p;
        if (c == '[') { while (*p == '[') p++; if (*p == 'L') while (*p != ';') p++; p++; kinds[n++] = 'L'; continue; }
        if (c == 'L') { while (*p != ';') p++; p++; kinds[n++] = 'L'; continue; }
        kinds[n++] = c; p++;
    }
    *ret = p[1] == '[' ? 'L' : p[1]; return n;
}

static int native_name(char* out, size_t cap, const char* cls, const char* name) {
    size_t n = snprintf(out, cap, "Java_");
    for (const char* c = cls; *c && n + 4 < cap; c++) { if (*c == '/') out[n++] = '_'; else if (*c == '_') { out[n++] = '_'; out[n++] = '1'; } else out[n++] = *c; }
    out[n++] = '_';
    for (const char* c = name; *c && n + 4 < cap; c++) { if (*c == '_') { out[n++] = '_'; out[n++] = '1'; } else out[n++] = *c; }
    out[n] = 0; return (int)n;
}
static u32 g_env_u32; static u32 g_exports_n; static struct { char* name; u32 addr; } g_exports[2048];
static u32 find_export(const char* name) { for (u32 i = 0; i < g_exports_n; i++) if (!strcmp(g_exports[i].name, name)) return g_exports[i].addr; return 0; }

// call a Java-visible native (an exported Java_* function) with JArgs, honouring the ARM EABI register/stack layout
static u64 call_java_native(u32 addr, u32 self, const char* kinds, int n, JArg* a, u32 sp) {
    u32 w[24]; int nw = 0; w[nw++] = U(g_env_obj); w[nw++] = self;
    for (int i = 0; i < n; i++) {
        char k = kinds[i];
        if (k == 'J' || k == 'D') { if (nw & 1) w[nw++] = 0; w[nw++] = (u32)a[i].j; w[nw++] = (u32)(a[i].j >> 32); }
        else if (k == 'F') w[nw++] = f2u(a[i].f);
        else w[nw++] = a[i].i;
    }
    u32 sp2 = (sp - 64 - 4 * 24) & ~7u;
    for (int i = 4; i < nw; i++) *(u32*)P(sp2 + 4 * (i - 4)) = w[i];
    return call_ind(addr, nw > 0 ? w[0] : 0, nw > 1 ? w[1] : 0, nw > 2 ? w[2] : 0, nw > 3 ? w[3] : 0, sp2);
}

/* Android asset directory iteration is ASCII-sorted. Host readdir() returns
 * filesystem/inode order, which differs from the original Android asset VFS
 * and can reorder model/texture registries. Sort the real asset names, never
 * substitute content or synthesize entries. */
static int ds_asset_name_compare(const void* aa, const void* bb) {
    JObj* a = OBJ(*(const u32*)aa);
    JObj* b = OBJ(*(const u32*)bb);
    const char* an = a && a->s ? a->s : "";
    const char* bn = b && b->s ? b->s : "";
    return strcmp(an, bn);
}

// ---- the Java methods the game expects from Android
/* Honor java.io.InputStream byte-array semantics. In particular, zero-length
 * reads return 0 (including at EOF), while a positive read at EOF is -1.
 * Never write outside the guest byte[] when a malformed JNI argument arrives. */
static int stream_read(JObj* o, u8* dst, int len) {
    if (len == 0) return 0;
    if (len < 0 || !o || !o->p || !dst) return -1;
    pthread_mutex_lock(&g_objlock);
    if (!o->p) { pthread_mutex_unlock(&g_objlock); return -1; }
    FILE* fp = (FILE*)o->p;
    const char* resource = o->s ? o->s : "";
    int is_m3g = getenv("DS_ASSET_TRACE") &&
        strlen(resource) >= 4 &&
        strcmp(resource + strlen(resource) - 4, ".m3g") == 0;
    long begin = is_m3g ? ftell(fp) : -1;
    size_t n = fread(dst, 1, (size_t)len, fp);
    if (is_m3g && begin == 0 && n >= 12) {
        fprintf(stderr, "[JAVA_REAL_M3G_READ] path=%s len=%zu header=",
                resource, n);
        for (int ds_i=0; ds_i<12; ds_i++)
            fprintf(stderr, "%02x ", (unsigned)dst[ds_i]);
        fprintf(stderr, "\n");
    }
    if (getenv("DS_ASSET_TRACE") && n != (size_t)len) {
        fprintf(stderr, "[JAVA_STREAM_PARTIAL] resource=%s request=%d got=%zu "
                "position=%ld eof=%d error=%d\n",
                o->s ? o->s : "(unknown)", len, n,
                ftell(fp), feof(fp), ferror(fp));
    }
    int result = n ? (int)n : -1;
    pthread_mutex_unlock(&g_objlock);
    return result;
}
static int stream_byte_array_ok(JObj* b, s32 offset, s32 count) {
    return b && b->kind == K_ARRAY && b->esz == 1 && b->p &&
        offset >= 0 && count >= 0 &&
        (u32)offset <= b->n && (u32)count <= b->n - (u32)offset;
}
/* Original Android AssetInputStream.skip never seeks past EOF or backwards.
 * fseek(SEEK_CUR) alone accepts off-end positions on desktop Linux, and the
 * old shim incorrectly reported bytes skipped beyond the resource's length.
 * The native M3G decoder relies on accurate stream positioning. */
static u64 stream_skip(JObj* stream, u64 requested_raw) {
    if (!stream || !stream->p) { g_exception = 1; return 0; }
    s64 requested = (s64)requested_raw;
    if (requested <= 0) return 0;
    FILE* fp = (FILE*)stream->p;
    flockfile(fp);
    long start = ftell(fp);
    if (start < 0 || fseek(fp, 0, SEEK_END) != 0) {
        g_exception = 1; funlockfile(fp); return 0;
    }
    long end = ftell(fp);
    if (end < 0 || fseek(fp, start, SEEK_SET) != 0) {
        g_exception = 1; funlockfile(fp); return 0;
    }
    u64 available = end > start ? (u64)(end - start) : 0;
    u64 actual = (u64)requested < available ? (u64)requested : available;
    if (fseek(fp, start + (long)actual, SEEK_SET) != 0) {
        g_exception = 1; funlockfile(fp); return 0;
    }
    funlockfile(fp);
    if (getenv("DS_ASSET_TRACE") && (u64)requested != actual)
        fprintf(stderr,
            "[JAVA_STREAM_SKIP_CLAMP] requested=%llu actual=%llu at=%ld file_size=%ld\n",
            (unsigned long long)requested, (unsigned long long)actual, start, end);
    return actual;
}
static u64 java_invoke(const Mid* m, u32 self, const char* kinds, int n, JArg* a, u32 sp, int* handled) {
    *handled = 1;
    JObj* so = OBJ(self);
    // methods that are implemented natively by the game library
    char nn[256]; native_name(nn, sizeof nn, m->cls, m->name);
    u32 ex = find_export(nn);
    if (ex) return call_java_native(ex, self, kinds, n, a, sp);
    const char* c = m->cls; const char* nm = m->name;
    if (!strcmp(c, "android/content/res/AssetManager")) {
        if (!strcmp(nm, "open")) {
            char path[1024]; JObj* s = OBJ(a[0].i); snprintf(path, sizeof path, "%s/%s", g_assets_root, s && s->s ? s->s : "");
            FILE* f = fopen(path, "rb");
            if (!f) { g_exception = 1; if (g_log_java) printf("   [java] AssetManager.open(%s) -> FileNotFoundException\n", path); return 0; }
            return new_obj(K_STREAM, path, f, 0);
        }
        if (!strcmp(nm, "openFd")) {
            char path[1024]; JObj* s = OBJ(a[0].i); snprintf(path, sizeof path, "%s/%s", g_assets_root, s && s->s ? s->s : "");
            FILE* f = fopen(path, "rb");
            if (g_log_java) printf("   [java] AssetManager.openFd(%s) -> %s\n", path, f ? "ok" : "FileNotFound");
            if (!f) { g_exception = 1; return 0; }
            fseek(f, 0, SEEK_END); u32 len = (u32)ftell(f); fseek(f, 0, SEEK_SET);
            /* The descriptor describes a resource; it is not a reusable FILE*.
             * Each createInputStream() call must get its own file position and
             * ownership. Sharing f between streams causes cross-thread seeks,
             * short reads, and double fclose() while decoding real M3G models. */
            fclose(f);
            u32 h = new_obj(K_AFD, "android/content/res/AssetFileDescriptor",
                            strdup(path), len);
            return h;
        }
        if (!strcmp(nm, "list")) {
            char path[1024]; JObj* s = OBJ(a[0].i); snprintf(path, sizeof path, "%s/%s", g_assets_root, s && s->s ? s->s : "");
            if (g_log_java) printf("   [java] AssetManager.list(%s)\n", path);
            DIR* d = opendir(path); if (!d) return 0;
            u32 names[1024]; u32 cnt = 0; struct dirent* e;
            while ((e = readdir(d)) && cnt < 1024) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) names[cnt++] = new_obj(K_STRING, e->d_name, 0, 0);
            closedir(d);
            qsort(names, cnt, sizeof names[0], ds_asset_name_compare);
            if (getenv("DS_ASSET_TRACE")) {
                fprintf(stderr, "[JAVA_ASSET_LIST] path=%s files=%u\n", path, cnt);
                for (u32 i = 0; i < cnt && i < 32; i++) {
                    JObj* item = OBJ(names[i]);
                    if (item && item->s) fprintf(stderr, "[JAVA_ASSET_ENTRY] %s\n", item->s);
                }
            }
            u32* arr = malloc(4 * (cnt + 1)); memcpy(arr, names, 4 * cnt);
            u32 h = new_obj(K_OBJARR, "[Ljava/lang/String;", arr, cnt); return h;
        }
    }
    if (!strcmp(c, "java/io/InputStream") && so && so->kind == K_STREAM) {
        if (!strcmp(nm, "read") && !strcmp(m->sig, "([BII)I")) {
            JObj* b = OBJ(a[0].i);
            s32 off = (s32)a[1].i, len = (s32)a[2].i;
            if (!stream_byte_array_ok(b, off, len)) { g_exception = 1; return (u32)-1; }
            return (u32)stream_read(so, (u8*)b->p + (u32)off, len);
        }
        if (!strcmp(nm, "read") && !strcmp(m->sig, "([B)I")) {
            JObj* b = OBJ(a[0].i);
            if (!b || b->n > 0x7fffffffu || !stream_byte_array_ok(b, 0, (s32)b->n)) {
                g_exception = 1; return (u32)-1;
            }
            return (u32)stream_read(so, (u8*)b->p, (int)b->n);
        }
        if (!strcmp(nm, "read") && !strcmp(m->sig, "()I")) {
            u8 ch = 0;
            return stream_read(so, &ch, 1) == 1 ? (u32)ch : (u32)-1;
        }
        if (!strcmp(nm, "close")) {
            pthread_mutex_lock(&g_objlock);
            if (so->p) { fclose((FILE*)so->p); so->p = 0; }
            pthread_mutex_unlock(&g_objlock);
            return 0;
        }
        if (!strcmp(nm, "available")) {
            if (!so->p) { g_exception = 1; return 0; }
            FILE* fp = (FILE*)so->p;
            flockfile(fp);
            long cur = ftell(fp);
            if (cur < 0 || fseek(fp, 0, SEEK_END) != 0) {
                g_exception = 1; funlockfile(fp); return 0;
            }
            long end = ftell(fp);
            if (fseek(fp, cur, SEEK_SET) != 0 || end < 0) {
                g_exception = 1; funlockfile(fp); return 0;
            }
            funlockfile(fp);
            return end > cur ? (u32)(end - cur) : 0;
        }
        if (!strcmp(nm, "skip")) return stream_skip(so, a[0].j);
    }
    if (!strcmp(c, "android/content/res/AssetFileDescriptor") && so && so->kind == K_AFD) {
        if (!strcmp(nm, "getStartOffset")) return 0;
        if (!strcmp(nm, "getLength") || !strcmp(nm, "getDeclaredLength")) return so->n;
        if (!strcmp(nm, "createInputStream")) {
            /* Independent fopen, not dup(): duplicated FDs share a file offset. */
            pthread_mutex_lock(&g_objlock);
            char* path = so->p ? strdup((const char*)so->p) : NULL;
            pthread_mutex_unlock(&g_objlock);
            FILE* stream = path ? fopen(path, "rb") : NULL;
            if (!stream) { free(path); g_exception = 1; return 0; }
            u32 h = new_obj(K_STREAM, path, stream, 0);
            free(path);
            return h;
        }
        if (!strcmp(nm, "close")) {
            pthread_mutex_lock(&g_objlock);
            free(so->p);
            so->p = NULL;
            pthread_mutex_unlock(&g_objlock);
            return 0;
        }
    }
    if (!strcmp(c, "com/ea/blast/MainActivity")) {
        if (!strcmp(nm, "getInstance") || !strcmp(nm, "GetInstance")) return g_activity;
        if (!strcmp(nm, "getAssets")) return g_assetmgr;
    }
    if (!strcmp(c, "com/ea/blast/SystemAndroidDelegate")) {
        static const struct { const char* n; const char* v; } tab[] = {
            {"GetAccelerometerCount", "1"}, {"GetApiLevel", "17"}, {"GetCameraCount", "0"}, {"GetChipset", ""},
            {"GetCompassCount", "0"}, {"GetDeviceModel", "PC"}, {"GetDeviceName", "pc"}, {"GetDeviceSubscriberID", ""},
            {"GetDeviceUniqueId", "-1"}, {"GetDisplayCount", "1"}, {"GetGyroscopeCount", "0"}, {"GetLanguage", "en"},
            {"GetLocale", "en_US"}, {"GetLocationAvailable", "false"}, {"GetManufacturer", "PC"}, {"GetMicrophoneCount", "0"},
            {"GetPhoneNumber", ""}, {"GetPhysicalKeyboardCount", "1"}, {"GetPlatformRawName", "Android"},
            {"GetPlatformStdName", "Android"}, {"GetPlatformVersion", "4.2"}, {"GetProcessorArchitecture", "armeabi-v7a armeabi"},
            {"GetTotalRAM", "-1"}, {"GetTouchPadCount", "0"}, {"GetTouchScreenCount", "1"}, {"GetTrackBallCount", "0"},
            {"GetVibratorCount", "0"}, {"GetVirtualKeyboardCount", "1"}, {"IsBatteryStateAvailable", "0"}};
        // Presented as a Sony Ericsson R800i (Xperia Play): Keyboard::Create then builds the game's gamepad keyboard
        // (it tests manufacturer starts with "sony" and model starts with "R800"). Override with DS_MANUFACTURER / DS_MODEL.
        const char* ov = 0;
        if (!strcmp(nm, "GetManufacturer")) ov = getenv("DS_MANUFACTURER") ? getenv("DS_MANUFACTURER") : "Sony Ericsson";
        if (!strcmp(nm, "GetDeviceModel")) ov = getenv("DS_MODEL") ? getenv("DS_MODEL") : "R800i";
        if (!strcmp(nm, "GetDeviceName")) ov = getenv("DS_DEVICE") ? getenv("DS_DEVICE") : "zeus";
        if (!strcmp(nm, "GetTouchPadCount")) ov = getenv("DS_TOUCHPADS") ? getenv("DS_TOUCHPADS") : "0";
        if (ov) return new_obj(K_STRING, ov, 0, 0);
        for (size_t i = 0; i < sizeof tab / sizeof *tab; i++) if (!strcmp(nm, tab[i].n)) return new_obj(K_STRING, tab[i].v, 0, 0);
        if (!strcmp(nm, "IntentView")) return 0;
    }
    if (!strcmp(c, "com/ea/blast/DisplayAndroidDelegate")) {
        const char* ew = getenv("DS_WIDTH"); const char* eh = getenv("DS_HEIGHT");
        if (!strcmp(nm, "GetDefaultWidth")) return ew ? atoi(ew) : 1280;
        if (!strcmp(nm, "GetDefaultHeight")) return eh ? atoi(eh) : 720;
        if (!strcmp(nm, "GetDpiX") || !strcmp(nm, "GetDpiY")) return f2u(160.0f);
        if (!strcmp(nm, "SetStdOrientation")) return 0;
    }
    if (!strcmp(c, "com/ea/blast/DeviceOrientationHandlerAndroidDelegate")) return 0;
    if (!strcmp(c, "com/ea/blast/AccelerometerAndroidDelegate")) return 0;      // fake accelerometer: enabled/frequency calls are accepted
    if (!strcmp(c, "com/eamobile/Query")) {
        if (!strcmp(nm, "isContentReady")) return 1;                         // the data is already on disk
        if (!strcmp(nm, "getVersion")) return new_obj(K_STRING, "1.0.1", 0, 0);
        if (!strcmp(nm, "getTotalMemory")) return 1024;                      // MB
        if (!strcmp(nm, "saveImage")) return 0;
    }
    if (!strcmp(c, "com/ea/blast/PowerManagerAndroid")) return 0;
    if (!strcmp(c, "com/ea/blast/TouchSurfaceAndroid") && !strcmp(nm, "IsTouchScreenMultiTouch")) return 1;
    if (!strcmp(c, "com/ea/blast/GetAppDataDirectoryDelegate") && !strcmp(m->sig, "()Ljava/lang/String;")) {
        const char* d = getenv("DS_DATA") ? getenv("DS_DATA") : "userdata"; mkdir(d, 0755);
        char full[4096]; if (!realpath(d, full)) snprintf(full, sizeof full, "%s", d);
        if (getenv("DS_ASSET_TRACE")) fprintf(stderr, "[JAVA_APP_STORAGE] %s -> %s\n", nm, full);
        return new_obj(K_STRING, full, 0, 0);
    }
    *handled = 0;
    if (g_log_java) printf("   [java] UNIMPLEMENTED %s.%s %s\n", c, nm, m->sig);
    return 0;
}

// ---- JNI function shims
static const char* sname(void) { return stub_names[g_stub_idx]; }
/* Real JNI stream shim regression, executed by the same game binary without
 * starting a game. This tests file ownership only; actual game boot remains
 * a separate required CI gate using the user's original APK. */
static int ds_test_asset_streams(void) {
    char root[]="/tmp/dspc-jni-stream-XXXXXX", path[256];
    if (!mkdtemp(root)) { perror("mkdtemp"); return 2; }
    snprintf(path, sizeof path, "%s/%s", root, "probe.dat");
    FILE* seed=fopen(path, "wb");
    if (!seed) { perror("fopen"); rmdir(root); return 2; }
    fwrite("abcdefgh", 1, 8, seed); fclose(seed);
    const char* previous_root=g_assets_root;
    g_assets_root=root;
    int handled=0, result=1;
    Mid afd={0}, create={0}, read_m={0}, close_m={0}, skip_m={0}, available_m={0};
    snprintf(afd.cls, sizeof afd.cls, "android/content/res/AssetManager");
    snprintf(afd.name, sizeof afd.name, "openFd");
    snprintf(afd.sig, sizeof afd.sig, "(Ljava/lang/String;)Landroid/content/res/AssetFileDescriptor;");
    snprintf(create.cls, sizeof create.cls, "android/content/res/AssetFileDescriptor");
    snprintf(create.name, sizeof create.name, "createInputStream");
    snprintf(create.sig, sizeof create.sig, "()Ljava/io/InputStream;");
    snprintf(read_m.cls, sizeof read_m.cls, "java/io/InputStream");
    snprintf(read_m.name, sizeof read_m.name, "read");
    snprintf(read_m.sig, sizeof read_m.sig, "([BII)I");
    snprintf(close_m.cls, sizeof close_m.cls, "java/io/InputStream");
    snprintf(close_m.name, sizeof close_m.name, "close");
    snprintf(close_m.sig, sizeof close_m.sig, "()V");
    snprintf(skip_m.cls, sizeof skip_m.cls, "java/io/InputStream");
    snprintf(skip_m.name, sizeof skip_m.name, "skip");
    snprintf(skip_m.sig, sizeof skip_m.sig, "(J)J");
    snprintf(available_m.cls, sizeof available_m.cls, "java/io/InputStream");
    snprintf(available_m.name, sizeof available_m.name, "available");
    snprintf(available_m.sig, sizeof available_m.sig, "()I");
    u32 path_obj=new_obj(K_STRING,"probe.dat",NULL,0);
    JArg path_arg[1]={{.i=path_obj}};
    u32 parent=(u32)java_invoke(&afd,0,"L",1,path_arg,0,&handled);
    JArg nothing[1]={{.i=0}};
    if (!parent) goto done;
    u32 a=(u32)java_invoke(&create,parent,"",0,nothing,0,&handled);
    u32 b=(u32)java_invoke(&create,parent,"",0,nothing,0,&handled);
    if (!a || !b || a==b) goto done;
    u32 abuf=new_obj(K_ARRAY,NULL,calloc(9,1),8);
    u32 bbuf=new_obj(K_ARRAY,NULL,calloc(9,1),8);
    OBJ(abuf)->esz=1;
    OBJ(bbuf)->esz=1;
    JArg ar[3]={{.i=abuf},{.i=0},{.i=3}};
    JArg br[3]={{.i=bbuf},{.i=0},{.i=3}};
    if ((u32)java_invoke(&read_m,a,"LII",3,ar,0,&handled)!=3) goto done;
    if ((u32)java_invoke(&read_m,b,"LII",3,br,0,&handled)!=3) goto done;
    if (memcmp(OBJ(abuf)->p,"abc",3) || memcmp(OBJ(bbuf)->p,"abc",3)) goto done;
    ar[1].i=3;
    if ((u32)java_invoke(&read_m,a,"LII",3,ar,0,&handled)!=3) goto done;
    if (memcmp(OBJ(abuf)->p,"abcdef",6)) goto done;
    java_invoke(&close_m,a,"",0,nothing,0,&handled);
    br[1].i=3;
    if ((u32)java_invoke(&read_m,b,"LII",3,br,0,&handled)!=3) goto done;
    if (memcmp(OBJ(bbuf)->p,"abcdef",6)) goto done;
    /* Closing the descriptor must not invalidate previously created streams. */
    snprintf(close_m.cls,sizeof close_m.cls,"android/content/res/AssetFileDescriptor");
    java_invoke(&close_m,parent,"",0,nothing,0,&handled);
    br[1].i=6; br[2].i=2;
    if ((u32)java_invoke(&read_m,b,"LII",3,br,0,&handled)!=2) goto done;
    if (memcmp(OBJ(bbuf)->p,"abcdefgh",8)) goto done;
    br[2].i=0;
    if ((u32)java_invoke(&read_m,b,"LII",3,br,0,&handled)!=0) goto done;
    br[2].i=1;
    if ((s32)java_invoke(&read_m,b,"LII",3,br,0,&handled)!=-1) goto done;
    if (java_invoke(&available_m,b,"",0,nothing,0,&handled)!=0) goto done;
    JArg skip_arg[1]={{.j=99}};
    if (java_invoke(&skip_m,b,"J",1,skip_arg,0,&handled)!=0) goto done;
    snprintf(close_m.cls,sizeof close_m.cls,"java/io/InputStream");
    java_invoke(&close_m,b,"",0,nothing,0,&handled);
    result=0;
done:
    g_assets_root=previous_root;
    unlink(path); rmdir(root);
    if(result) fprintf(stderr,"[JNI_STREAM_SELFTEST] FAIL independent cursor/close/EOF semantics\n");
    else fprintf(stderr,"[JNI_STREAM_SELFTEST] PASS independent cursor/close/EOF semantics\n");
    return result;
}

static u64 jni_default(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n);
    static const char* seen[256]; static int ns;
    /* Background texture workers may call unimplemented JNI functions too.
     * This dedup cache is shared only for logging, so synchronize it with
     * the already-recursive Java object/cache mutex. */
    pthread_mutex_lock(&g_objlock);
    int dup = 0; for (int i = 0; i < ns; i++) if (seen[i] == n) dup = 1;
    if (!dup && ns < 256) { seen[ns++] = n; if (g_log_java) printf("   [jni] UNIMPLEMENTED %s (r1=%#x r2=%#x r3=%#x)\n", n, r1, r2, r3); }
    pthread_mutex_unlock(&g_objlock);
    return 0;
}
static u64 jni_GetVersion(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetVersion"); RET(0x10006); }
static u64 jni_FindClass(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:FindClass"); RET(class_handle(P(r1))); }
static u64 jni_GetObjectClass(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr(sname()); RET(class_handle(cls_of(r1))); }
static u64 jni_ref(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr(sname()); RET(r1); }
static u64 jni_noop(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr(sname()); RET(0); }
static u64 jni_exc_check(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr(sname()); RET(g_exception ? 1 : 0); }
static u64 jni_exc_clear(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr(sname()); g_exception = 0; RET(0); }
static u64 jni_id(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n);
    Mid* m = calloc(1, sizeof *m); snprintf(m->cls, sizeof m->cls, "%s", cls_of(r1)); snprintf(m->name, sizeof m->name, "%s", (char*)P(r2));
    snprintf(m->sig, sizeof m->sig, "%s", (char*)P(r3)); m->is_static = strstr(n, "Static") != 0;
    if (!strstr(n, "Field") && g_log_java) printf("   [jni] %s %s.%s %s\n", n + 4, m->cls, m->name, m->sig);
    RET(U(m));
}
static u64 jni_RegisterNatives(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:RegisterNatives"); g_natives += (int)r3; RET(0); }
static u64 jni_NewStringUTF(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:NewStringUTF"); RET(r1 ? new_obj(K_STRING, P(r1), 0, 0) : 0); }
static u64 jni_GetStringUTFChars(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetStringUTFChars"); JObj* o = OBJ(r1); if (r2) *(u8*)P(r2) = 0; RET(o ? U(o->s) : 0); }
static u64 jni_GetStringUTFLength(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetStringUTFLength"); JObj* o = OBJ(r1); RET(o && o->s ? strlen(o->s) : 0); }
static u64 jni_GetArrayLength(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetArrayLength"); JObj* o = OBJ(r1); RET(o ? o->n : 0); }

static int esz_of(const char* n) {              // element size from e.g. "JNI:GetByteArrayRegion"
    if (strstr(n, "Boolean") || strstr(n, "Byte")) return 1; if (strstr(n, "Char") || strstr(n, "Short")) return 2;
    if (strstr(n, "Long") || strstr(n, "Double")) return 8; return 4;
}
static u64 jni_NewArray(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n); int esz = esz_of(n);
    u32 h = new_obj(K_ARRAY, 0, calloc(r1 ? r1 : 1, esz), r1); OBJ(h)->esz = (u8)esz; RET(h);
}
static u64 jni_ArrayElements(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n); JObj* o = OBJ(r1);
    int esz = esz_of(n);
    if (!o || o->kind != K_ARRAY || o->esz != esz || !o->p) {
        g_exception = 1;
        if (getenv("DS_ASSET_TRACE"))
            fprintf(stderr, "[JNI_ARRAY_ELEMENTS_BAD] method=%s handle=%08x\n", n, r1);
        RET(0);
    }
    if (r2) *(u8*)P(r2) = 0; /* array is pinned, no copied storage */
    if (getenv("DS_ASSET_TRACE") && esz==1 && o->n>=12) {
        const unsigned char* ptr=(const unsigned char*)o->p;
        if (ptr[0]==0xab && ptr[1]==0x49 && ptr[2]==0x4d) {
            fprintf(stderr, "[JNI_M3G_ARRAY_ELEMENTS] tid=%lu name=%s handle=%08x data=%p size=%u head=",
                    (unsigned long)pthread_self(),n,(unsigned)r1,o->p,(unsigned)o->n);
            for (int i=0;i<12;i++) fprintf(stderr,"%02x ",ptr[i]);
            fputc('\n',stderr);
        }
    }
    RET(U(o->p));
}
static u64 jni_ArrayRegionGet(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n); JObj* o = OBJ(r1); int esz = esz_of(n); u32 buf = *(u32*)P(sp);
    if (!o || o->kind != K_ARRAY || o->esz != esz || !o->p ||
        r2 > o->n || r3 > o->n-r2 || !buf) {
        g_exception=1;
        if (getenv("DS_ASSET_TRACE"))
            fprintf(stderr, "[JNI_ARRAY_REGION_BAD] method=%s handle=%08x offset=%u len=%u buffer=%08x\n",
                    n, r1, r2, r3, buf);
        RET(0);
    }
    memcpy(P(buf), (u8*)o->p + (size_t)r2 * esz, (size_t)r3 * esz);
    if (getenv("DS_ASSET_TRACE") && esz==1 && r3==12) {
        const u8* d=P(buf);
        fprintf(stderr, "[JNI_12_BYTE_READ] tid=%lu dest=%08x array=%08x off=%u bytes=",
                (unsigned long)pthread_self(),buf,r1,r2);
        for(int i=0;i<12;i++) fprintf(stderr,"%02x ",(unsigned)d[i]);
        fprintf(stderr,"\n");
    }
    RET(0);
}
static u64 jni_ArrayRegionSet(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n);
    JObj* o = OBJ(r1); int esz = esz_of(n); u32 buf = *(u32*)P(sp);
    if (!o || o->kind != K_ARRAY || o->esz != esz || !o->p ||
        r2 > o->n || r3 > o->n - r2 || (r3 && !buf)) {
        g_exception=1;
        if (getenv("DS_ASSET_TRACE"))
            fprintf(stderr,
                "[JNI_ARRAY_REGION_SET_BAD] method=%s handle=%08x offset=%u len=%u arraylen=%u buffer=%08x\n",
                n,r1,r2,r3,o ? o->n : 0,buf);
        RET(0);
    }
    if (r3) memcpy((u8*)o->p + (size_t)r2 * esz, P(buf), (size_t)r3 * esz);
    if (getenv("DS_ASSET_TRACE") && esz==1 && r3==12) {
        const u8* d=(const u8*)o->p + (size_t)r2;
        fprintf(stderr,"[JNI_12_BYTE_SET] tid=%lu array=%08x off=%u bytes=",
            (unsigned long)pthread_self(),r1,r2);
        for(int i=0;i<12;i++) fprintf(stderr,"%02x ",(unsigned)d[i]);
        fprintf(stderr,"\n");
    }
    RET(0);
}
static u64 jni_NewObjectArray(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:NewObjectArray"); u32* arr = calloc(r1 + 1, 4); RET(new_obj(K_OBJARR, "[Ljava/lang/Object;", arr, r1)); }
static u64 jni_GetObjectArrayElement(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetObjectArrayElement"); JObj* o = OBJ(r1); RET(o && r2 < o->n ? ((u32*)o->p)[r2] : 0); }
static u64 jni_SetObjectArrayElement(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:SetObjectArrayElement"); JObj* o = OBJ(r1); if (o && r2 < o->n) ((u32*)o->p)[r2] = r3; RET(0); }

// ---- Call<Type>Method / CallStatic<Type>Method / ...V / ...A
static u64 jni_call(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    const char* n = sname(); tr(n);
    Mid* m = (Mid*)P(r2);
    char kinds[32], ret; int na = parse_sig(m->sig, kinds, &ret);
    int variant = n[strlen(n) - 1] == 'V' && strstr(n, "MethodV") ? 1 : (n[strlen(n) - 1] == 'A' && strstr(n, "MethodA") ? 2 : 0);
    JArg a[32]; memset(a, 0, sizeof a);
    u32 regs[4] = {r0, r1, r2, r3}; AR ar;
    if (variant == 0) { ar.regs = regs; ar.nregs = 4; ar.ri = 3; ar.mem = (const u32*)P(sp); }
    else if (variant == 1) { ar.regs = 0; ar.nregs = 0; ar.ri = 0; ar.mem = (const u32*)P(r3); }
    for (int i = 0; i < na; i++) {
        char k = kinds[i];
        if (variant == 2) { const u8* j = (const u8*)P(r3) + 8 * i; if (k == 'J' || k == 'D') memcpy(&a[i].j, j, 8); else if (k == 'F') memcpy(&a[i].f, j, 4); else memcpy(&a[i].i, j, 4); continue; }
        if (k == 'J') a[i].j = ar64(&ar);
        else if (k == 'D') { u64 b = ar64(&ar); memcpy(&a[i].d, &b, 8); }
        else if (k == 'F') { u64 b = ar64(&ar); double d; memcpy(&d, &b, 8); a[i].f = (float)d; }
        else a[i].i = ar32(&ar);
    }
    int handled; u32 self = r1;
    if (g_log_java) printf("   [java] call %s.%s %s (self=%#x)\n", m->cls, m->name, m->sig, self);
    u64 rv = java_invoke(m, self, kinds, na, a, sp, &handled);
    if (g_log_java) printf("   [java]   -> %#llx\n", (unsigned long long)rv);
    if (ret == 'V') return 0;
    return rv;
}

// ---- UTF-16 <-> UTF-8 (the game builds Java strings from wide characters)
static u32 utf8_to_utf16(const char* s, u16* out) {           // returns number of UTF-16 units; out may be NULL
    u32 n = 0; const u8* p = (const u8*)s;
    while (*p) {
        u32 c = *p++;
        if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
        else if (c >= 0xE0 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
        else if (c >= 0xC0 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
        if (c >= 0x10000) { c -= 0x10000; if (out) { out[n] = (u16)(0xD800 + (c >> 10)); out[n + 1] = (u16)(0xDC00 + (c & 0x3FF)); } n += 2; }
        else { if (out) out[n] = (u16)c; n++; }
    }
    return n;
}
static char* utf16_to_utf8(const u16* w, u32 len) {
    char* o = malloc(len * 3 + 1); u32 n = 0;
    for (u32 i = 0; i < len; i++) {
        u32 c = w[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < len && w[i + 1] >= 0xDC00 && w[i + 1] < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00); i++; }
        if (c < 0x80) o[n++] = (char)c;
        else if (c < 0x800) { o[n++] = (char)(0xC0 | (c >> 6)); o[n++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o[n++] = (char)(0xE0 | (c >> 12)); o[n++] = (char)(0x80 | ((c >> 6) & 63)); o[n++] = (char)(0x80 | (c & 63)); }
        else { o[n++] = (char)(0xF0 | (c >> 18)); o[n++] = (char)(0x80 | ((c >> 12) & 63)); o[n++] = (char)(0x80 | ((c >> 6) & 63)); o[n++] = (char)(0x80 | (c & 63)); }
    }
    o[n] = 0; return o;
}
static u64 jni_NewString(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    tr("JNI:NewString"); char* s = utf16_to_utf8((const u16*)P(r1), r2); u32 h = new_obj(K_STRING, s, 0, 0); free(s); RET(h);
}
static u64 jni_GetStringLength(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetStringLength"); JObj* o = OBJ(r1); RET(o && o->s ? utf8_to_utf16(o->s, 0) : 0); }
static u64 jni_GetStringChars(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    tr("JNI:GetStringChars"); JObj* o = OBJ(r1); if (!o || !o->s) RET(0);
    u32 n = utf8_to_utf16(o->s, 0); u16* w = malloc(2 * (n + 1)); utf8_to_utf16(o->s, w); w[n] = 0;
    if (r2) *(u8*)P(r2) = 1; RET(U(w));
}
static u64 jni_ReleaseStringChars(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:ReleaseStringChars"); free(P(r2)); RET(0); }
static u64 jni_PopLocalFrame(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:PopLocalFrame"); RET(r1); }
static u64 jni_NewObject(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) {
    tr(sname()); Mid* m = (Mid*)P(r2);
    if (g_log_java) printf("   [java] new %s %s\n", cls_of(r1), m ? m->sig : "");
    RET(new_obj(K_OBJECT, cls_of(r1), 0, 0));
}
static u64 jni_GetJavaVM(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JNI:GetJavaVM"); *(u32*)P(r1) = U(g_vm_obj); RET(0); }
static u64 jvm_GetEnv(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr("JVM:GetEnv"); *(u32*)P(r1) = U(g_env_obj); RET(0); }
static u64 jvm_default(u32 r0, u32 r1, u32 r2, u32 r3, u32 sp) { tr(sname()); return 0; }

static u32 jni_tab[256], jvm_tab[8];
static int has(const char* n, const char* s) { return strstr(n, s) != 0; }
static void build_jni(void) {
    for (u32 i = 0; i < JNI_N; i++) {
        const char* n = JNI_NAMES[i];
        char* full = malloc(64); snprintf(full, 64, "JNI:%s", n);
        fn_t f = jni_default;
        if (!strcmp(n, "GetVersion")) f = jni_GetVersion;
        else if (!strcmp(n, "FindClass")) f = jni_FindClass;
        else if (!strcmp(n, "GetObjectClass")) f = jni_GetObjectClass;
        else if (!strcmp(n, "NewGlobalRef") || !strcmp(n, "NewLocalRef") || !strcmp(n, "NewWeakGlobalRef")) f = jni_ref;
        else if (!strcmp(n, "DeleteLocalRef") || !strcmp(n, "DeleteGlobalRef") || !strcmp(n, "DeleteWeakGlobalRef")) f = jni_noop;
        else if (!strcmp(n, "ExceptionCheck") || !strcmp(n, "ExceptionOccurred")) f = jni_exc_check;
        else if (!strcmp(n, "ExceptionClear")) f = jni_exc_clear;
        else if (!strcmp(n, "GetStaticMethodID") || !strcmp(n, "GetMethodID") || !strcmp(n, "GetFieldID") || !strcmp(n, "GetStaticFieldID")) f = jni_id;
        else if (!strcmp(n, "RegisterNatives")) f = jni_RegisterNatives;
        else if (!strcmp(n, "GetJavaVM")) f = jni_GetJavaVM;
        else if (!strcmp(n, "NewString")) f = jni_NewString;
        else if (!strcmp(n, "GetStringLength")) f = jni_GetStringLength;
        else if (!strcmp(n, "GetStringChars")) f = jni_GetStringChars;
        else if (!strcmp(n, "ReleaseStringChars")) f = jni_ReleaseStringChars;
        else if (!strcmp(n, "PopLocalFrame")) f = jni_PopLocalFrame;
        else if (!strcmp(n, "PushLocalFrame") || !strcmp(n, "MonitorEnter") || !strcmp(n, "MonitorExit") || !strcmp(n, "EnsureLocalCapacity")) f = jni_noop;
        else if (!strcmp(n, "NewStringUTF")) f = jni_NewStringUTF;
        else if (!strcmp(n, "GetStringUTFChars")) f = jni_GetStringUTFChars;
        else if (!strcmp(n, "ReleaseStringUTFChars")) f = jni_noop;
        else if (!strcmp(n, "GetStringUTFLength")) f = jni_GetStringUTFLength;
        else if (!strcmp(n, "GetArrayLength")) f = jni_GetArrayLength;
        else if (!strcmp(n, "NewObjectArray")) f = jni_NewObjectArray;
        else if (!strcmp(n, "GetObjectArrayElement")) f = jni_GetObjectArrayElement;
        else if (!strcmp(n, "SetObjectArrayElement")) f = jni_SetObjectArrayElement;
        else if (has(n, "Array") && !has(n, "Object")) {
            if (!strncmp(n, "New", 3)) f = jni_NewArray;
            else if (has(n, "ArrayElements") && !strncmp(n, "Get", 3)) f = jni_ArrayElements;
            else if (has(n, "ArrayElements")) f = jni_noop;
            else if (has(n, "ArrayRegion") && !strncmp(n, "Get", 3)) f = jni_ArrayRegionGet;
            else if (has(n, "ArrayRegion")) f = jni_ArrayRegionSet;
        }
        else if (!strncmp(n, "Call", 4) && has(n, "Method") && !has(n, "Nonvirtual")) f = jni_call;
        else if (!strncmp(n, "NewObject", 9) && !has(n, "Array")) f = jni_NewObject;
        jni_tab[i] = make_stub(full, f);
    }
    static const char* jvm_names[8] = {"JVM:reserved0", "JVM:reserved1", "JVM:reserved2", "JVM:DestroyJavaVM", "JVM:AttachCurrentThread",
                                       "JVM:DetachCurrentThread", "JVM:GetEnv", "JVM:AttachCurrentThreadAsDaemon"};
    for (int i = 0; i < 8; i++) jvm_tab[i] = make_stub(jvm_names[i], i == 6 ? jvm_GetEnv : (i == 4 || i == 7 ? jvm_GetEnv : jvm_default));
    g_env_obj[0] = U(jni_tab); g_vm_obj[0] = U(jvm_tab);
}
