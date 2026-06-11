#include "kernel.h"
#include "output.h"

#include <stdio.h>
#include <windows.h>

void print_kernel_info(void)
{
    char buildStr[64] = {0};
    DWORD ubr = 0;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(buildStr);
        if (RegQueryValueExA(hKey, "CurrentBuildNumber", NULL, NULL, (LPBYTE)buildStr, &size) != ERROR_SUCCESS)
            buildStr[0] = '\0';

        size = sizeof(ubr);
        if (RegQueryValueExA(hKey, "UBR", NULL, NULL, (LPBYTE)&ubr, &size) != ERROR_SUCCESS)
            ubr = 0;

        RegCloseKey(hKey);
    }

    if (buildStr[0]) {
        char buf[128];
        if (ubr > 0)
            snprintf(buf, sizeof(buf), "WIN32_NT 10.0.%s.%lu", buildStr, (unsigned long)ubr);
        else
            snprintf(buf, sizeof(buf), "WIN32_NT 10.0.%s", buildStr);
        print_block("Kernel", buf);
    }
}
