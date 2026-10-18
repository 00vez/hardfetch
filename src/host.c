#include "host.h"
#include "debug.h"

#include <string.h>
#include <windows.h>

static void reg_str(HKEY root, const char* subkey, const char* value,
                    char* out, size_t outLen)
{
    DWORD len = (DWORD)outLen;
    HKEY hKey;
    size_t n;
    out[0] = '\0';
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    if (RegQueryValueExA(hKey, value, NULL, NULL, (LPBYTE)out, &len) != ERROR_SUCCESS)
        out[0] = '\0';
    RegCloseKey(hKey);
    n = strlen(out);
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
        out[--n] = '\0';
}

void host_collect(hf_state* s)
{
    debug_begin("host");
    reg_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
            "BaseBoardProduct", s->host, sizeof(s->host));
    debug_log("BaseBoardProduct = '%s'", s->host);
    debug_end();
}