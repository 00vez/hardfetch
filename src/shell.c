#include "shell.h"
#include "debug.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winver.h>


#pragma comment(lib, "version.lib")

static void lower_copy(wchar_t* dst, size_t n, const wchar_t* src)
{
    size_t i;
    wcsncpy_s(dst, n, src, _TRUNCATE);
    for (i = 0; dst[i]; i++)
        if (dst[i] >= L'A' && dst[i] <= L'Z') dst[i] += 32;
}

/* NtQueryInformationProcess gives the parent pid directly; a
   CreateToolhelp32Snapshot walk costs several milliseconds per call and the
   old code took one snapshot per ancestor. */
typedef LONG (WINAPI* PFN_NtQueryInformationProcess)(HANDLE, int, PVOID, ULONG, PULONG);

static DWORD parent_of(DWORD pid)
{
    static PFN_NtQueryInformationProcess fn = NULL;
    static int resolved = 0;
    typedef struct { ULONG_PTR ExitStatus; PVOID PebBaseAddress;
                     ULONG_PTR AffinityMask; LONG BasePriority;
                     ULONG_PTR UniqueProcessId; ULONG_PTR InheritedFromUniqueProcessId; } BASIC_INFO;
    BASIC_INFO info;

    if (!resolved) {
        HMODULE nt = GetModuleHandleA("ntdll.dll");
        resolved = 1;
        if (nt)
            fn = (PFN_NtQueryInformationProcess)GetProcAddress(nt, "NtQueryInformationProcess");
    }
    if (!fn) return 0;

    {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        DWORD parent = 0;
        if (!h) return 0;
        ZeroMemory(&info, sizeof(info));
        if (fn(h, 0 /*ProcessBasicInformation*/, &info, sizeof(info), NULL) == 0)
            parent = (DWORD)(ULONG_PTR)info.InheritedFromUniqueProcessId;
        CloseHandle(h);
        return parent;
    }
}

static bool exe_of_pid(DWORD pid, wchar_t* out, size_t outLen)
{
    HANDLE h;
    DWORD len = (DWORD)outLen;

    out[0] = 0;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    if (!QueryFullProcessImageNameW(h, 0, out, &len)) {
        CloseHandle(h);
        out[0] = 0;
        return false;
    }
    CloseHandle(h);
    return true;
}

/* Shells and console hosts that sit between the terminal and the program. */
static bool is_intermediate(const wchar_t* lowerExe)
{
    static const wchar_t* names[] = {
        L"pwsh.exe", L"powershell.exe", L"cmd.exe", L"bash.exe", L"zsh.exe",
        L"conhost.exe", L"openconsole.exe", L"mintty.exe", L"sh.exe", L"fish.exe",
        L"nu.exe", L"pwsh-preview.exe", L"windowsterminal.exe"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (wcsstr(lowerExe, names[i])) return true;
    return false;
}

static const char* shell_from_exe(const wchar_t* lowerExe)
{
    if (wcsstr(lowerExe, L"pwsh-preview")) return "PowerShell Preview";
    if (wcsstr(lowerExe, L"pwsh")) return "PowerShell";
    if (wcsstr(lowerExe, L"powershell")) return "Windows PowerShell";
    if (wcsstr(lowerExe, L"cmd")) return "cmd";
    if (wcsstr(lowerExe, L"bash")) return "bash";
    if (wcsstr(lowerExe, L"zsh")) return "zsh";
    if (wcsstr(lowerExe, L"fish")) return "fish";
    if (wcsstr(lowerExe, L"nu")) return "nushell";
    return "";
}

static void reg_ps_version(const char* subkey, char* out, size_t outLen)
{
    HKEY hKey;
    DWORD size = (DWORD)outLen;
    out[0] = '\0';
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    if (RegQueryValueExA(hKey, "PowerShellVersion", NULL, NULL, (LPBYTE)out, &size) != ERROR_SUCCESS)
        out[0] = '\0';
    RegCloseKey(hKey);
}

static void file_version(const wchar_t* path, char* out, size_t outLen)
{
    DWORD handle = 0;
    DWORD size;
    void* buf;
    size_t len;

    out[0] = '\0';
    size = GetFileVersionInfoSizeExW(FILE_VER_GET_LOCALISED, path, &handle);
    if (size == 0) return;
    buf = malloc(size);
    if (!buf) return;
    if (GetFileVersionInfoExW(FILE_VER_GET_LOCALISED, path, handle, size, buf)) {
        VS_FIXEDFILEINFO* info = NULL;
        UINT infoSize = 0;
        if (VerQueryValueW(buf, L"\\", (void**)&info, &infoSize) && info) {
            unsigned int parts[4];
            parts[0] = HIWORD(info->dwFileVersionMS);
            parts[1] = LOWORD(info->dwFileVersionMS);
            parts[2] = HIWORD(info->dwFileVersionLS);
            parts[3] = LOWORD(info->dwFileVersionLS);
            len = 0;
            {
                int last = 3;
                while (last > 0 && parts[last] == 0) last--;
                for (int i = 0; i <= last; i++)
                    len += (size_t)snprintf(out + len, outLen - len, "%s%u", i ? "." : "", parts[i]);
            }
        }
    }
    free(buf);
}

static void trim_trailing_zeros(char* v)
{
    size_t len = strlen(v);
    while (len > 1 && v[len - 1] == '0') {
        if (v[len - 2] == '.') { len--; break; }
        len--;
    }
    while (len > 0 && v[len - 1] == '.') len--;
    v[len] = '\0';
}

void shell_collect(hf_state* s)
{
    DWORD pid = GetCurrentProcessId();
    wchar_t exePath[MAX_PATH];
    wchar_t lower[MAX_PATH];

    debug_begin("shell");

    snprintf(s->shell_name, sizeof(s->shell_name), "%s", "");
    s->shell_version[0] = '\0';
    exePath[0] = L'\0';

    /* Walk up until a non-shell ancestor is found. */
    for (int depth = 0; depth < 8 && pid != 0; depth++) {
        DWORD parent = parent_of(pid);
        wchar_t candidate[MAX_PATH];
        if (parent == 0) break;
        if (!exe_of_pid(parent, candidate, MAX_PATH)) break;
        lower_copy(lower, MAX_PATH, candidate);
        debug_log("ancestor %lu '%ls'", (unsigned long)parent, lower);
        if (!is_intermediate(lower)) {
            wcsncpy_s(exePath, MAX_PATH, candidate, _TRUNCATE);
            break;
        }
        pid = parent;
    }

    if (exePath[0]) {
        const char* name = shell_from_exe(lower);
        if (name[0]) {
            snprintf(s->shell_name, sizeof(s->shell_name), "%s", name);
            if (wcsstr(lower, L"pwsh")) {
                reg_ps_version("SOFTWARE\\Microsoft\\PowerShellCore\\3\\PowerShellEngine",
                               s->shell_version, sizeof(s->shell_version));
                if (!s->shell_version[0]) file_version(exePath, s->shell_version, sizeof(s->shell_version));
            } else if (wcsstr(lower, L"powershell")) {
                reg_ps_version("SOFTWARE\\Microsoft\\PowerShell\\3\\PowerShellEngine",
                               s->shell_version, sizeof(s->shell_version));
                if (!s->shell_version[0])
                    reg_ps_version("SOFTWARE\\Microsoft\\PowerShell\\1\\PowerShellEngine",
                                   s->shell_version, sizeof(s->shell_version));
            } else {
                file_version(exePath, s->shell_version, sizeof(s->shell_version));
            }
            trim_trailing_zeros(s->shell_version);
            debug_log("shell = '%s' '%s'", s->shell_name, s->shell_version);
        } else {
            debug_log("ancestor '%ls' is not a known shell", lower);
        }
    } else {
        debug_log("no non-shell ancestor found");
    }

    debug_end();
}