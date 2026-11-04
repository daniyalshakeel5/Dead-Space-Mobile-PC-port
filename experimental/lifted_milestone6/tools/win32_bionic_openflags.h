/* 32-bit Android Bionic open flags -> native Win32 C runtime flags.
 * Pure Win32 adapter shared by the actual lifted guest host and a native
 * MSVC runtime test; no game source or original assets.
 */
#pragma once
#ifndef _WIN32
#error This header is Windows-only
#endif
#include <fcntl.h>
#define DS_GUEST_O_CREAT   0x0040u
#define DS_GUEST_O_EXCL    0x0080u
#define DS_GUEST_O_TRUNC   0x0200u
#define DS_GUEST_O_APPEND  0x0400u
static __inline int ds_win32_from_guest_openflags(unsigned int f) {
    int result;
    switch (f & 3u) {
      case 0: result = _O_RDONLY; break;
      case 1: result = _O_WRONLY; break;
      case 2: result = _O_RDWR; break;
      default: return -1;
    }
    if (f & DS_GUEST_O_CREAT) result |= _O_CREAT;
    if (f & DS_GUEST_O_EXCL) result |= _O_EXCL;
    if (f & DS_GUEST_O_TRUNC) result |= _O_TRUNC;
    if (f & DS_GUEST_O_APPEND) result |= _O_APPEND;
    return result | _O_BINARY;
}
