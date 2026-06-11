#include "host.h"
#include "output.h"

#include <stdio.h>
#include <windows.h>

void print_host_info(void)
{
    char product[256] = {0};

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\BIOS",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(product);
        RegQueryValueExA(hKey, "BaseBoardProduct", NULL, NULL, (LPBYTE)product, &size);
        RegCloseKey(hKey);
    }

    if (product[0])
        print_block("Host", product);
}
