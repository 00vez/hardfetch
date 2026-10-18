#include "kernel.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef LONG (WINAPI* PFN_RtlGetVersion)(OSVERSIONINFOEXW*);

static DWORD read_ubr(void)
{
    HKEY hKey;
    DWORD ubr = 0, size = sizeof(ubr);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;
    RegQueryValueExA(hKey, "UBR", NULL, NULL, (LPBYTE)&ubr, &size);
    RegCloseKey(hKey);
    return ubr;
}

void kernel_collect_build(hf_state* s)
{
    debug_begin("build");
    s->build_ubr = read_ubr();
    debug_log("UBR = %llu -> build string '%lu.%llu'",
              s->build_ubr, s->os_build, s->build_ubr);
    debug_end();
}

/* RtlGetVersion reports the real version; GetVersionEx is shimmed by the
   application compatibility database and reports 6.2 for anything else. */
void kernel_collect_kernel(hf_state* s)
{
    HMODULE nt;
    PFN_RtlGetVersion rtl;
    OSVERSIONINFOEXW vi;

    debug_begin("kernel");

    /* Wine masquerades as WIN32_NT; ntdll exports wine_get_version when present. */
    nt = GetModuleHandleA("ntdll.dll");
    if (nt) {
        const char* (WINAPI* wine_get_version)(void);
        wine_get_version = (const char* (WINAPI*)(void))GetProcAddress(nt, "wine_get_version");
        if (wine_get_version) {
            const char* w = wine_get_version();
            snprintf(s->kernel_name, sizeof(s->kernel_name), "Wine_%s", w ? w : "?");
            debug_log("Wine detected: '%s'", w ? w : "?");
        } else {
            debug_log("no wine_get_version export, assuming native Windows");
        }
    }

    ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    rtl = (PFN_RtlGetVersion)GetProcAddress(nt, "RtlGetVersion");
    if (!rtl || rtl(&vi) != 0) {
        debug_log("RtlGetVersion unavailable, falling back to registry build");
        s->kernel_major = 10;
        s->kernel_minor = 0;
        s->kernel_build = s->os_build;
    } else {
        s->kernel_major = vi.dwMajorVersion;
        s->kernel_minor = vi.dwMinorVersion;
        s->kernel_build = vi.dwBuildNumber;
        debug_log("RtlGetVersion -> %u.%u build %lu (%ls)",
                  vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber, vi.szCSDVersion);
        if (s->kernel_build != s->os_build)
            debug_log("build %lu differs from registry CurrentBuildNumber %lu",
                      s->kernel_build, s->os_build);
    }

    debug_end();
}