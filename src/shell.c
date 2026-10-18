#include "shell.h"
#include "debug.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winver.h>

#pragma comment(lib, "version.lib")

typedef LONG(WINAPI* PFN_NtQueryInformationProcess)(HANDLE, int, PVOID, ULONG, PULONG);

typedef struct {
    ULONG_PTR ExitStatus;
    PVOID     PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG      BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} HF_PROCESS_BASIC_INFORMATION;

static void lower_copy(wchar_t* dst, size_t n, const wchar_t* src)
{
    size_t i;
    wcsncpy_s(dst, n, src, _TRUNCATE);
    for (i = 0; dst[i]; i++)
        if (dst[i] >= L'A' && dst[i] <= L'Z') dst[i] += 32;
}

/* NtQueryInformationProcess gives the parent pid directly.
   CreateToolhelp32Snapshot walks every process and cost several milliseconds
   per call; the old code took one snapshot per ancestor. */
static DWORD parent_of(DWORD pid)
{
    static PFN_NtQueryInformationProcess fn = NULL;
    static int resolved = 0;
    HF_PROCESS_BASIC_INFORMATION info;
    HANDLE h;
    DWORD parent = 0;

    if (!resolved) {
        HMODULE nt = GetModuleHandleA("ntdll.dll");
        resolved = 1;
        if (nt)
            fn = (PFN_NtQueryInformationProcess)GetProcAddress(nt, "NtQueryInformationProcess");
    }
    if (!fn) return 0;

    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    ZeroMemory(&info, sizeof(info));
    if (fn(h, 0 /*ProcessBasicInformation*/, &info, sizeof(info), NULL) == 0)
        parent = (DWORD)(ULONG_PTR)info.InheritedFromUniqueProcessId;
    CloseHandle(h);
    return parent;
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

/* Console hosts and terminal emulators that sit between the shell and us. */
static bool is_conhost_or_terminal(const wchar_t* lowerExe)
{
    static const wchar_t* names[] = {
        L"conhost.exe", L"openconsole.exe", L"windowsterminal.exe",
        L"conemu.exe", L"mintty.exe", L"wt.exe"
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
    if (wcsstr(lowerExe, L"nu.exe")) return "nushell";
    return 0;
}

static void reg_ps_version(const char* subkey, char* out, size_t outLen)
{
    HKEY hKey;
    DWORD size = (DWORD)outLen;

    out[0] = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    if (RegQueryValueExA(hKey, "PowerShellVersion", NULL, NULL, (LPBYTE)out, &size) != ERROR_SUCCESS)
        out[0] = 0;
    RegCloseKey(hKey);
}

static void file_version(const wchar_t* path, char* out, size_t outLen)
{
    DWORD handle = 0;
    DWORD size;
    void* buf;

    out[0] = 0;
    if (!path || !path[0]) return;
    size = GetFileVersionInfoSizeExW(FILE_VER_GET_LOCALISED, path, &handle);
    if (size == 0) return;
    buf = malloc(size);
    if (!buf) return;
    if (GetFileVersionInfoExW(FILE_VER_GET_LOCALISED, path, handle, size, buf)) {
        VS_FIXEDFILEINFO* info = NULL;
        UINT infoSize = 0;
        if (VerQueryValueW(buf, L"\\", (void**)&info, &infoSize) && info) {
            unsigned int parts[4];
            size_t len = 0;
            int last = 3;
            parts[0] = HIWORD(info->dwFileVersionMS);
            parts[1] = LOWORD(info->dwFileVersionMS);
            parts[2] = HIWORD(info->dwFileVersionLS);
            parts[3] = LOWORD(info->dwFileVersionLS);
            while (last > 0 && parts[last] == 0) last--;
            for (int i = 0; i <= last && len + 16 < outLen; i++)
                len += (size_t)snprintf(out + len, outLen - len, "%s%u",
                                        i ? "." : "", parts[i]);
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
    v[len] = 0;
}

void shell_collect(hf_state* s)
{
    DWORD pid = GetCurrentProcessId();
    wchar_t shellExe[MAX_PATH];
    wchar_t lower[MAX_PATH];
    int foundShell = 0;

    debug_begin("shell");

    s->shell_name[0] = 0;
    s->shell_version[0] = 0;
    shellExe[0] = 0;
    lower[0] = 0;

    /* Walk the ancestor chain once. Two answers come out of it: the nearest
       ancestor that is a shell, and the nearest one that is neither a shell
       nor a console host (the terminal emulator). Skipping over shells while
       hunting for the terminal must not hide the shell itself. */
    for (int depth = 0; depth < 8 && pid != 0; depth++) {
        DWORD parent = parent_of(pid);
        wchar_t candidate[MAX_PATH];
        wchar_t lowerLocal[MAX_PATH];
        const char* name;

        if (parent == 0) break;
        if (!exe_of_pid(parent, candidate, MAX_PATH)) break;
        lower_copy(lowerLocal, MAX_PATH, candidate);
        debug_log("ancestor %lu '%ls'", (unsigned long)parent, lowerLocal);

        /* shell_from_exe returns NULL for anything that is not a shell. */
        name = shell_from_exe(lowerLocal);
        if (name != NULL && !foundShell) {
            foundShell = 1;
            snprintf(s->shell_name, sizeof(s->shell_name), "%s", name);
            wcsncpy_s(shellExe, MAX_PATH, candidate, _TRUNCATE);
            lower_copy(lower, MAX_PATH, candidate);
        }

        /* Reached the terminal emulator: nothing above it is interesting. */
        if (name == NULL && !is_conhost_or_terminal(lowerLocal))
            break;

        pid = parent;
    }

    if (foundShell) {
        if (wcsstr(lower, L"pwsh")) {
            reg_ps_version("SOFTWARE\\Microsoft\\PowerShellCore\\3\\PowerShellEngine",
                           s->shell_version, sizeof(s->shell_version));
            if (!s->shell_version[0])
                file_version(shellExe, s->shell_version, sizeof(s->shell_version));
        } else if (wcsstr(lower, L"powershell")) {
            reg_ps_version("SOFTWARE\\Microsoft\\PowerShell\\3\\PowerShellEngine",
                           s->shell_version, sizeof(s->shell_version));
            if (!s->shell_version[0])
                reg_ps_version("SOFTWARE\\Microsoft\\PowerShell\\1\\PowerShellEngine",
                               s->shell_version, sizeof(s->shell_version));
        } else {
            file_version(shellExe, s->shell_version, sizeof(s->shell_version));
        }
        trim_trailing_zeros(s->shell_version);
        debug_log("shell = '%s' '%s' from '%ls'",
                  s->shell_name, s->shell_version, shellExe);
    } else {
        debug_log("no known shell among the ancestors");
    }

    debug_end();
}