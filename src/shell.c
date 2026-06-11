#include "shell.h"
#include "output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winver.h>
#include <tlhelp32.h>

#pragma comment(lib, "version.lib")

static void lower_str(wchar_t* s)
{
    for (; *s; s++) { if (*s >= L'A' && *s <= L'Z') *s += 32; }
}

static int get_parent_full_path(wchar_t* outPath, size_t outPathSize)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return -1;

    DWORD ourPid = GetCurrentProcessId();
    DWORD parentPid = 0;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == ourPid) {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (!parentPid) return -1;

    HANDLE hParent = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parentPid);
    if (!hParent) return -1;

    DWORD pathLen = (DWORD)outPathSize;
    int ret = QueryFullProcessImageNameW(hParent, 0, outPath, &pathLen) ? 0 : -1;
    CloseHandle(hParent);
    return ret;
}

static const char* detect_shell_name(wchar_t* fullPath, size_t fullPathSize)
{
    if (get_parent_full_path(fullPath, fullPathSize) != 0) return "";

    wchar_t copy[1024];
    wcsncpy_s(copy, 1024, fullPath, _TRUNCATE);
    lower_str(copy);

    if (wcsstr(copy, L"pwsh"))
        return "PowerShell";
    if (wcsstr(copy, L"powershell"))
        return "Windows PowerShell";
    if (wcsstr(copy, L"cmd"))
        return "cmd";
    if (wcsstr(copy, L"bash"))
        return "bash";
    if (wcsstr(copy, L"wsl"))
        return "WSL";
    return "";
}

static void get_file_version(const wchar_t* exePath, char* outVer, size_t outVerSize)
{
    outVer[0] = '\0';
    DWORD handle = 0;
    DWORD verSize = GetFileVersionInfoSizeExW(FILE_VER_GET_LOCALISED, exePath, &handle);
    if (verSize == 0) return;

    void* verBuf = malloc(verSize);
    if (!verBuf) return;

    if (GetFileVersionInfoExW(FILE_VER_GET_LOCALISED, exePath, handle, verSize, verBuf)) {
        VS_FIXEDFILEINFO* vinfo = NULL;
        UINT vinfoSize = 0;
        if (VerQueryValueW(verBuf, L"\\", (void**)&vinfo, &vinfoSize) && vinfo) {
            unsigned int parts[4];
            parts[0] = (vinfo->dwFileVersionMS >> 16) & 0xFFFF;
            parts[1] = vinfo->dwFileVersionMS & 0xFFFF;
            parts[2] = (vinfo->dwFileVersionLS >> 16) & 0xFFFF;
            parts[3] = vinfo->dwFileVersionLS & 0xFFFF;

            int last = 2;
            while (last > 0 && parts[last] == 0) last--;
            int pos = 0;
            for (int i = 0; i <= last; i++)
                pos += snprintf(outVer + pos, outVerSize - pos, "%s%u", i > 0 ? "." : "", parts[i]);
        }
    }

    free(verBuf);
}

static int get_ps_version_from_reg(const char* subkey, char* outVer, size_t outVerSize)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return -1;
    DWORD size = (DWORD)outVerSize;
    int ret = -1;
    if (RegQueryValueExA(hKey, "PowerShellVersion", NULL, NULL, (LPBYTE)outVer, &size) == ERROR_SUCCESS)
        ret = 0;
    RegCloseKey(hKey);
    return ret;
}

void print_shell_info(void)
{
    wchar_t fullPath[1024] = {0};
    const char* shell = detect_shell_name(fullPath, 1024);
    if (!shell[0]) return;

    int is_pwsh = 0;
    {
        wchar_t copy[1024];
        wcsncpy_s(copy, 1024, fullPath, _TRUNCATE);
        lower_str(copy);
        is_pwsh = (wcsstr(copy, L"pwsh") != NULL);
    }

    char version[128] = {0};

    if (is_pwsh) {
        get_ps_version_from_reg("SOFTWARE\\Microsoft\\PowerShellCore\\3\\PowerShellEngine",
                                version, sizeof(version));
        if (!version[0])
            get_file_version(fullPath, version, sizeof(version));
    } else if (strcmp(shell, "Windows PowerShell") == 0) {
        get_ps_version_from_reg("SOFTWARE\\Microsoft\\PowerShell\\3\\PowerShellEngine",
                                version, sizeof(version));
        if (!version[0])
            get_ps_version_from_reg("SOFTWARE\\Microsoft\\PowerShell\\1\\PowerShellEngine",
                                    version, sizeof(version));
    } else {
        get_file_version(fullPath, version, sizeof(version));
    }

    size_t len = strlen(version);
    while (len > 0 && (version[len - 1] == '0' || version[len - 1] == '.')) {
        if (len > 1 && version[len - 2] == '.' && version[len - 1] == '0')
            { len--; }
        else if (version[len - 1] == '.')
            { len--; }
        else
            break;
    }
    version[len] = '\0';

    char buf[256];
    if (version[0])
        snprintf(buf, sizeof(buf), "%s %s", shell, version);
    else
        snprintf(buf, sizeof(buf), "%s", shell);
    print_block("Shell", buf);
}
