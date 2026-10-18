#include "os_info.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void os_collect(hf_state* s)
{
    HKEY hKey;
    DWORD size;
    SYSTEM_INFO si;

    debug_begin("os");

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        size = (DWORD)sizeof(s->os_product);
        if (RegQueryValueExA(hKey, "ProductName", NULL, NULL,
                (LPBYTE)s->os_product, &size) != ERROR_SUCCESS)
            s->os_product[0] = '\0';
        size = (DWORD)sizeof(s->os_display_version);
        if (RegQueryValueExA(hKey, "DisplayVersion", NULL, NULL,
                (LPBYTE)s->os_display_version, &size) != ERROR_SUCCESS)
            s->os_display_version[0] = '\0';
        {
            /* REG_SZ, not REG_DWORD: reading it as a DWORD yields zero. */
            char buildStr[16];
            DWORD t = 0;
            size = sizeof(buildStr);
            if (RegQueryValueExA(hKey, "CurrentBuildNumber", NULL, &t,
                    (LPBYTE)buildStr, &size) == ERROR_SUCCESS && t == REG_SZ)
                s->os_build = strtoul(buildStr, NULL, 10);
        }
        RegCloseKey(hKey);
    } else {
        debug_log("RegOpenKeyExA(CurrentVersion) failed: %s", debug_win32(GetLastError()));
    }

    debug_log("ProductName='%s' DisplayVersion='%s' CurrentBuildNumber=%lu",
              s->os_product, s->os_display_version, s->os_build);

    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: strcpy_s(s->os_arch, sizeof(s->os_arch), "x86_64"); break;
        case PROCESSOR_ARCHITECTURE_ARM64: strcpy_s(s->os_arch, sizeof(s->os_arch), "aarch64"); break;
        case PROCESSOR_ARCHITECTURE_INTEL: strcpy_s(s->os_arch, sizeof(s->os_arch), "x86"); break;
        default: strcpy_s(s->os_arch, sizeof(s->os_arch), "unknown"); break;
    }
    debug_log("NativeSystemInfo arch -> '%s'", s->os_arch);

    /* ProductName keeps saying "Windows 10" on Windows 11. */
    if (s->os_build >= 22000 && s->os_product[0]) {
        char* w10 = strstr(s->os_product, "Windows 10");
        if (w10) memcpy_s(w10, strlen(w10), "Windows 11", 10);
    }

    debug_log("resolved product = '%s'", s->os_product);
    debug_end();
}