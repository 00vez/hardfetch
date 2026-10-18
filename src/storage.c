#include "storage.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static void resolve_system_volume(char* out, size_t outLen)
{
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);

    out[0] = '\0';
    if (n == 0 || n >= MAX_PATH) {
        snprintf(out, outLen, "C:\\");
        return;
    }

    /* X:\path\to\hardfetch.exe -> X:\ */
    {
        size_t i = 0;
        while (exe[i] && i < 2) i++;      /* skip drive letter and ':' */
        if (i < 2 || exe[0] == '\0') {
            snprintf(out, outLen, "C:\\");
            return;
        }
        if (WideCharToMultiByte(CP_ACP, 0, exe, 3, out, (int)outLen, NULL, NULL) == 0)
            snprintf(out, outLen, "C:\\");
    }
    debug_log("executable path '%ls' -> volume '%s'", exe, out);
}

void storage_collect(hf_state* s)
{
    ULARGE_INTEGER freeBytes, totalBytes;

    debug_begin("disk");

    resolve_system_volume(s->disk_volume, sizeof(s->disk_volume));

    if (GetDiskFreeSpaceExA(s->disk_volume, &freeBytes, &totalBytes, NULL)) {
        s->disk_total = (unsigned long long)totalBytes.QuadPart;
        s->disk_used = s->disk_total - (unsigned long long)freeBytes.QuadPart;
        debug_log("volume=%s total=%llu used=%llu free=%llu",
                  s->disk_volume, s->disk_total, s->disk_used,
                  (unsigned long long)freeBytes.QuadPart);
    } else {
        debug_log("GetDiskFreeSpaceExA(%s) failed: %s",
                  s->disk_volume, debug_win32(GetLastError()));
    }

    debug_end();
}