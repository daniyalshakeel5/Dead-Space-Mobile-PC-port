/* Win32 versions of Android/Bionic imports seen in the owner's private
 * 2026-11-04 link log. Included inside host.c, only for 32-bit Windows.
 * Preserve guest ARM softfp five-word dispatch ABI; no original game code.
 * Do not replace C++ unwinding with silent successful stubs.
 */
#ifndef _WIN32
#error This file is a Windows-only host adapter
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>
#include <limits.h>
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384u
#endif

/* Windows i686 uses 32-bit pointer parameters for sscanf conversion outputs.
 * All scanf variadic parameters are output pointers, unlike printf, so native
 * CRT scanf can consume the guest pointers without ARM FP argument conversion.
 * ARM AAPCS supplies r2,r3 first and remaining 32-bit pointers on guest stack.
 */
static int ds_guest_scan(const char *source, const char *fmt, const u32 *a) {
    return sscanf(source, fmt, P(a[0]), P(a[1]), P(a[2]), P(a[3]),
                  P(a[4]), P(a[5]), P(a[6]), P(a[7]),
                  P(a[8]), P(a[9]), P(a[10]), P(a[11]));
}
IMP(sscanf) {
    T(sscanf);
    u32 a[12] = {r2, r3};
    const u32 *rest = (const u32*)P(sp);
    for (int i = 2; i < 12; i++) a[i] = rest[i - 2];
    int rc = ds_guest_scan((const char*)P(r0), (const char*)P(r1), a);
    RET(rc);
}
IMP(vsscanf) {
    T(vsscanf);
    /* 32-bit Bionic va_list is a pointer to the guest's stacked words. */
    if (!r2) { g_errno = EINVAL; RET(-1); }
    RET(ds_guest_scan((const char*)P(r0), (const char*)P(r1), (const u32*)P(r2)));
}

/* Android's dirent is 280 bytes; never expose MinGW's host struct layout. */
IMP(readdir_r) {
    T(readdir_r);
    if (!r0 || !r1 || !r2) RET(EINVAL);
    errno = 0;
    struct dirent *e = readdir((DIR*)P(r0));
    if (!e) {
        *(u32*)P(r2) = 0;
        RET(errno);
    }
    u8 *b = (u8*)P(r1);
    memset(b, 0, 280);
    *(u64*)(b + 0) = (u64)e->d_ino;
    *(u16*)(b + 16) = 280;
    b[18] = 0; /* MinGW d_type is not reliable. */
    strncpy((char*)(b + 19), e->d_name, 255);
    *(u32*)P(r2) = r1;
    RET(0);
}

/* The translated engine creates a *separate* guest stack per native worker
 * (threads.c:thread_entry). Windows cannot adopt an arbitrary Android stack
 * address, but retaining the caller-specified stack size is meaningful. */
IMP(pthread_attr_setstack) {
    T(pthread_attr_setstack);
    if (!r0 || !r1 || r2 < PTHREAD_STACK_MIN) RET(EINVAL);
    u32 *attr = (u32*)P(r0);
    attr[2] = r2;
    attr[3] = r1; /* retained for diagnostic fidelity */
    RET(0);
}

/* The translated ARM EHABI unwinder has not been ported to Win32. Silently
 * returning from these functions would corrupt exception state. Stop with
 * an explicit diagnostic if game code actually enters ARM exception cleanup. */
IMP(__gnu_Unwind_Find_exidx) {
    T(__gnu_Unwind_Find_exidx);
    trap_unsupported(0, "ARM EHABI __gnu_Unwind_Find_exidx on Win32");
    RET(0);
}
IMP(__cxa_begin_cleanup) {
    T(__cxa_begin_cleanup);
    trap_unsupported(0, "ARM C++ exception cleanup on Win32");
    RET(0);
}
IMP(__cxa_type_match) {
    T(__cxa_type_match);
    trap_unsupported(0, "ARM C++ exception type matching on Win32");
    RET(0);
}

/* Winsock socket handles are 32 bits in this native i686 build. */
static INIT_ONCE ds_wsa_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK ds_wsa_startup(PINIT_ONCE o, PVOID p, PVOID *c) {
    WSADATA w; (void)o; (void)p; (void)c;
    return WSAStartup(MAKEWORD(2,2), &w) == 0;
}
static int ds_wsa_ready(void) {
    if (InitOnceExecuteOnce(&ds_wsa_once, ds_wsa_startup, NULL, NULL)) return 1;
    g_errno = EIO;
    return 0;
}
static int ds_wsa_fail(void) {
    int e = WSAGetLastError();
    switch(e) {
        case WSAEWOULDBLOCK: g_errno = EAGAIN; break;
        case WSAECONNRESET: g_errno = ECONNRESET; break;
        case WSAECONNREFUSED: g_errno = ECONNREFUSED; break;
        case WSAETIMEDOUT: g_errno = ETIMEDOUT; break;
        case WSAEINTR: g_errno = EINTR; break;
        case WSAEINVAL: g_errno = EINVAL; break;
        default: g_errno = EIO; break;
    }
    return -1;
}
IMP(socket) {
    T(socket);
    if (!ds_wsa_ready()) RET(-1);
    SOCKET s = socket((int)r0, (int)r1, (int)r2);
    if (s == INVALID_SOCKET) RET(ds_wsa_fail());
    RET((u32)s);
}
IMP(bind) {
    T(bind);
    if (!ds_wsa_ready()) RET(-1);
    int n = bind((SOCKET)r0, (const struct sockaddr*)P(r1), (int)r2);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(connect) {
    T(connect);
    if (!ds_wsa_ready()) RET(-1);
    int n = connect((SOCKET)r0, (const struct sockaddr*)P(r1), (int)r2);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(send) {
    T(send);
    if (!ds_wsa_ready()) RET(-1);
    int n = send((SOCKET)r0, (const char*)P(r1), (int)r2, (int)r3);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(recv) {
    T(recv);
    if (!ds_wsa_ready()) RET(-1);
    int n = recv((SOCKET)r0, (char*)P(r1), (int)r2, (int)r3);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(sendto) {
    T(sendto);
    if (!ds_wsa_ready()) RET(-1);
    const u32 *extra = (const u32*)P(sp);
    int n = sendto((SOCKET)r0, (const char*)P(r1), (int)r2, (int)r3,
                   (const struct sockaddr*)P(extra[0]), (int)extra[1]);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(recvfrom) {
    T(recvfrom);
    if (!ds_wsa_ready()) RET(-1);
    const u32 *extra = (const u32*)P(sp);
    int n = recvfrom((SOCKET)r0, (char*)P(r1), (int)r2, (int)r3,
                     (struct sockaddr*)P(extra[0]), (int*)P(extra[1]));
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(shutdown) {
    T(shutdown);
    if (!ds_wsa_ready()) RET(-1);
    int n = shutdown((SOCKET)r0, (int)r1);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(getpeername) {
    T(getpeername);
    if (!ds_wsa_ready()) RET(-1);
    int n = getpeername((SOCKET)r0, (struct sockaddr*)P(r1), (int*)P(r2));
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
IMP(getsockname) {
    T(getsockname);
    if (!ds_wsa_ready()) RET(-1);
    int n = getsockname((SOCKET)r0, (struct sockaddr*)P(r1), (int*)P(r2));
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}
static int ds_android_socket_option(int opt) {
    switch(opt) {
        case 2: return SO_REUSEADDR;
        case 6: return SO_BROADCAST;
        case 7: return SO_SNDBUF;
        case 8: return SO_RCVBUF;
        case 9: return SO_KEEPALIVE;
        case 13: return SO_LINGER;
        default: return opt;
    }
}
IMP(setsockopt) {
    T(setsockopt);
    if (!ds_wsa_ready()) RET(-1);
    const u32 *extra = (const u32*)P(sp);
    /* Android SOL_SOCKET=1, Winsock SOL_SOCKET=0xffff. */
    int level = r1 == 1 ? SOL_SOCKET : (int)r1;
    int option = r1 == 1 ? ds_android_socket_option((int)r2) : (int)r2;
    int n = setsockopt((SOCKET)r0, level, option, (const char*)P(r3), (int)extra[0]);
    RET(n == SOCKET_ERROR ? ds_wsa_fail() : n);
}

/* Android addrinfo has ai_addr BEFORE ai_canonname; Winsock has the reverse.
 * Copy to guest-layout nodes to avoid silently exposing Windows ABI layout. */
typedef struct {
    u32 flags, family, socktype, protocol, addrlen;
    u32 addr, canonname, next;
} DsAndroidAddrinfo;
IMP(getaddrinfo) {
    T(getaddrinfo);
    if (!r3) RET(EAI_FAIL);
    *(u32*)P(r3) = 0;
    if (!ds_wsa_ready()) RET(EAI_FAIL);
    const DsAndroidAddrinfo *in = r2 ? (const DsAndroidAddrinfo*)P(r2) : NULL;
    struct addrinfo hints, *native = NULL;
    struct addrinfo *hintp = NULL;
    if (in) {
        memset(&hints, 0, sizeof hints);
        hints.ai_flags = (int)in->flags;
        hints.ai_family = (int)in->family;
        hints.ai_socktype = (int)in->socktype;
        hints.ai_protocol = (int)in->protocol;
        hintp = &hints;
    }
    int rc = getaddrinfo((const char*)P(r0), (const char*)P(r1), hintp, &native);
    if (rc) RET(rc);
    DsAndroidAddrinfo *first = NULL, *prev = NULL;
    for (struct addrinfo *a = native; a; a = a->ai_next) {
        DsAndroidAddrinfo *g = (DsAndroidAddrinfo*)calloc(1, sizeof *g);
        if (!g) { rc = EAI_MEMORY; break; }
        g->flags = a->ai_flags; g->family = a->ai_family;
        g->socktype = a->ai_socktype; g->protocol = a->ai_protocol;
        g->addrlen = (u32)a->ai_addrlen;
        if (a->ai_addr && g->addrlen) {
            void *cp = malloc(g->addrlen);
            if (!cp) { free(g); rc = EAI_MEMORY; break; }
            memcpy(cp, a->ai_addr, g->addrlen);
            g->addr = U(cp);
        }
        if (a->ai_canonname) {
            char *cp = strdup(a->ai_canonname);
            if (!cp) { free(P(g->addr)); free(g); rc = EAI_MEMORY; break; }
            g->canonname = U(cp);
        }
        if (prev) prev->next = U(g); else first = g;
        prev = g;
    }
    freeaddrinfo(native);
    if (rc) {
        DsAndroidAddrinfo *g = first;
        while (g) {
            DsAndroidAddrinfo *next = (DsAndroidAddrinfo*)P(g->next);
            free(P(g->addr)); free(P(g->canonname)); free(g); g = next;
        }
        RET(rc);
    }
    *(u32*)P(r3) = U(first);
    RET(0);
}
IMP(freeaddrinfo) {
    T(freeaddrinfo);
    DsAndroidAddrinfo *g = (DsAndroidAddrinfo*)P(r0);
    while (g) {
        DsAndroidAddrinfo *next = (DsAndroidAddrinfo*)P(g->next);
        free(P(g->addr)); free(P(g->canonname)); free(g); g = next;
    }
    RET(0);
}
