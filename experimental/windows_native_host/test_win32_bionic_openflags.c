/* Executed by GitHub Actions on actual Windows x86 MSVC.
 * Confirms first-run Android save creates, appends, truncates and EXCL.
 * Tests only the independently written Bionic/CRT flag translator.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <errno.h>
#include <string.h>
#include "../lifted_milestone6/tools/win32_bionic_openflags.h"

int main(void) {
    char tempdir[MAX_PATH], path[MAX_PATH], got[8] = {0};
    int fd, rc;
    assert(GetTempPathA(MAX_PATH, tempdir) != 0);
    assert(GetTempFileNameA(tempdir, "dsp", 0, path) != 0);
    assert(DeleteFileA(path) != 0);

    assert(ds_win32_from_guest_openflags(3u) == -1);
    assert((ds_win32_from_guest_openflags(0x40u | 1u) & _O_CREAT) != 0);
    assert((ds_win32_from_guest_openflags(0x400u | 1u) & _O_APPEND) != 0);
    assert((ds_win32_from_guest_openflags(0x80u | 1u) & _O_EXCL) != 0);
    assert((ds_win32_from_guest_openflags(0x200u | 2u) & _O_TRUNC) != 0);

    fd = _open(path, ds_win32_from_guest_openflags(DS_GUEST_O_CREAT | 1u), _S_IREAD | _S_IWRITE);
    assert(fd >= 0);
    assert(_write(fd, "A", 1) == 1);
    assert(_close(fd) == 0);

    fd = _open(path, ds_win32_from_guest_openflags(DS_GUEST_O_APPEND | 1u), _S_IREAD | _S_IWRITE);
    assert(fd >= 0);
    assert(_write(fd, "B", 1) == 1);
    assert(_close(fd) == 0);
    fd = _open(path, ds_win32_from_guest_openflags(0u), _S_IREAD);
    assert(fd >= 0);
    assert(_read(fd, got, 2) == 2);
    assert(_close(fd) == 0);
    assert(memcmp(got, "AB", 2) == 0);

    errno = 0;
    rc = _open(path, ds_win32_from_guest_openflags(DS_GUEST_O_CREAT | DS_GUEST_O_EXCL | 1u), _S_IREAD | _S_IWRITE);
    assert(rc == -1 && errno == EEXIST);

    fd = _open(path, ds_win32_from_guest_openflags(DS_GUEST_O_TRUNC | 2u), _S_IREAD | _S_IWRITE);
    assert(fd >= 0);
    assert(_close(fd) == 0);
    fd = _open(path, ds_win32_from_guest_openflags(0u), _S_IREAD);
    assert(fd >= 0);
    assert(_read(fd, got, 2) == 0);
    assert(_close(fd) == 0);
    assert(DeleteFileA(path) != 0);
    puts("PASS: Win32 ARM Bionic O_CREAT/O_EXCL/O_APPEND/O_TRUNC/READ flags (real x86 file I/O)");
    return 0;
}
