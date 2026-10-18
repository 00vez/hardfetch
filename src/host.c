#include "host.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static void reg_str(HKEY root, const char* subkey, const char* value,
                    char* out, size_t outLen)
{
    DWORD len = (DWORD)outLen;
    HKEY hKey;
    size_t n;

    out[0] = 0;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    if (RegQueryValueExA(hKey, value, NULL, NULL, (LPBYTE)out, &len) != ERROR_SUCCESS)
        out[0] = 0;
    RegCloseKey(hKey);
    n = strlen(out);
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
        out[--n] = 0;
}

void ident_collect(hf_state* s)
{
    wchar_t wbuf[256];
    DWORD n;

    debug_begin("ident");

    snprintf(s->user_name, sizeof(s->user_name), "%s", "user");
    n = (DWORD)(sizeof(wbuf) / sizeof(wbuf[0]));
    wbuf[0] = 0;
    if (GetUserNameW(wbuf, &n))
        WideCharToMultiByte(CP_UTF8, 0, wbuf, -1,
                            s->user_name, (int)sizeof(s->user_name), NULL, NULL);

    snprintf(s->host_name, sizeof(s->host_name), "%s", "unknown");
    n = (DWORD)(sizeof(wbuf) / sizeof(wbuf[0]));
    wbuf[0] = 0;
    if (GetComputerNameExW(ComputerNameDnsHostname, wbuf, &n) && wbuf[0]) {
        WideCharToMultiByte(CP_UTF8, 0, wbuf, -1,
                            s->host_name, (int)sizeof(s->host_name), NULL, NULL);
    } else {
        n = (DWORD)(sizeof(wbuf) / sizeof(wbuf[0]));
        wbuf[0] = 0;
        if (GetComputerNameW(wbuf, &n) && wbuf[0])
            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1,
                                s->host_name, (int)sizeof(s->host_name), NULL, NULL);
    }

    debug_log("user='%s' host='%s'", s->user_name, s->host_name);
    debug_end();
}

void host_collect(hf_state* s)
{
    debug_begin("host");
    reg_str(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
            "BaseBoardProduct", s->host, sizeof(s->host));
    debug_log("BaseBoardProduct = '%s'", s->host);
    debug_end();
}
