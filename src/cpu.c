#include "cpu.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <powrprof.h>

/* PROCESSOR_POWER_INFORMATION is declared in winnt.h only for some SDK targets. */
typedef struct {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} HF_PROCESSOR_POWER_INFORMATION;
#include <intrin.h>

#pragma comment(lib, "powrprof.lib")

/* ---------- load measurement window ---------- */

static FILETIME s_idle1, s_kernel1, s_user1;
static int s_window_open = 0;

void cpu_sample_begin(void)
{
    s_window_open = GetSystemTimes(&s_idle1, &s_kernel1, &s_user1) ? 1 : 0;
}

static ULONGLONG ft(const FILETIME* f)
{
    ULARGE_INTEGER u;
    u.LowPart = f->dwLowDateTime;
    u.HighPart = f->dwHighDateTime;
    return u.QuadPart;
}

void cpu_sample_end(hf_state* s)
{
    FILETIME idle2, kernel2, user2;
    ULONGLONG idleD, kernelD, userD, totalD;

    s->cpu_load = 0.0;

    debug_begin("load");
    if (!s_window_open) { debug_log("no window"); debug_end(); return; }
    s_window_open = 0;

    if (!GetSystemTimes(&idle2, &kernel2, &user2)) {
        debug_log("GetSystemTimes failed");
        debug_end();
        return;
    }

    idleD = ft(&idle2) - ft(&s_idle1);
    kernelD = ft(&kernel2) - ft(&s_kernel1);
    userD = ft(&user2) - ft(&s_user1);
    totalD = kernelD + userD;          /* kernel time already includes idle */

    if (totalD == 0) { debug_log("no usable delta"); debug_end(); return; }

    s->cpu_load = 1.0 - (double)idleD / (double)totalD;
    if (s->cpu_load < 0.0) s->cpu_load = 0.0;
    if (s->cpu_load > 1.0) s->cpu_load = 1.0;

    debug_log("window idle=%llu kernel=%llu user=%llu total=%llu -> %.4f",
              idleD, kernelD, userD, totalD, s->cpu_load);
    debug_end();
}

/* ---------- static identity ---------- */

static void rtrim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static void cpuid(unsigned int leaf, unsigned int* a, unsigned int* b,
                  unsigned int* c, unsigned int* d)
{
    int r[4];
    __cpuidex(r, (int)leaf, 0);
    *a = (unsigned int)r[0];
    *b = (unsigned int)r[1];
    *c = (unsigned int)r[2];
    *d = (unsigned int)r[3];
}

static void read_boost_and_cores(hf_state* s)
{
    unsigned int a, b, c, d;

    /* CPUID leaf 0x16: EAX = base, EBX = max. Both are 0 on many systems
       and 0 whenever a hypervisor reports zero frequencies. */
    unsigned int maxLeaf;
    cpuid(0, &a, &b, &c, &d);
    maxLeaf = a;
    debug_log("CPUID.0 raw eax=0x%X ebx=0x%X ecx=0x%X edx=0x%X", a, b, c, d);
    cpuid(0x16, &a, &b, &c, &d);
    debug_log("CPUID.16 base=%u max=%u bus=%u (advertised max leaf was 0x%X)",
              a, b, c, maxLeaf);
    if (b > 0) s->cpu_boost_mhz = b;
    if (a > 0) s->cpu_base_mhz = a;

    /* Physical cores and threads via GetLogicalProcessorInformationEx,
       which reports processor groups correctly; GetSystemInfo caps at 64. */
    {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buf = NULL;
        DWORD len = 0;
        if (GetLogicalProcessorInformationEx(RelationAll, NULL, &len) ||
            GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            buf = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(len);
            if (buf && GetLogicalProcessorInformationEx(RelationAll, buf, &len)) {
                PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX p = buf;
                while ((BYTE*)p < (BYTE*)buf + len) {
                    if (p->Relationship == RelationProcessorCore) {
                        s->cpu_cores++;
                        {
                            KAFFINITY mask = p->Processor.GroupMask[0].Mask;
                            unsigned int n = 0;
                            while (mask) { n += (unsigned int)(mask & 1); mask >>= 1; }
                            s->cpu_threads += (int)n;
                        }
                    }
                    p = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)
                        ((BYTE*)p + p->Size);
                }
            }
            free(buf);
        }
        if (s->cpu_threads == 0) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            s->cpu_threads = (int)si.dwNumberOfProcessors;
        }
        debug_log("cores=%d threads=%d", s->cpu_cores, s->cpu_threads);
    }
}

void cpu_collect_static(hf_state* s)
{
    HKEY hKey;
    DWORD size;

    debug_begin("cpu");

    size = (DWORD)sizeof(s->cpu_name);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
                (LPBYTE)s->cpu_name, &size) != ERROR_SUCCESS)
            s->cpu_name[0] = '\0';
        RegCloseKey(hKey);
        rtrim(s->cpu_name);
    } else {
        debug_log("CentralProcessor\\0 open failed: %s", debug_win32(GetLastError()));
    }
    {
        /* Registry reports "AMD Ryzen 7 7800X3D 8-Core Processor". */
        char* suffix = strstr(s->cpu_name, "-Core");
        if (suffix) {
            char* p = suffix - 1;
            while (p > s->cpu_name && *p >= '0' && *p <= '9') p--;
            if (p > s->cpu_name && *p == ' ') *p = 0;
            else if (p == s->cpu_name) s->cpu_name[0] = 0;
        }
    }
    debug_log("ProcessorNameString = '%s'", s->cpu_name);

    read_boost_and_cores(s);
    cpu_collect_live(s);
    debug_end();
}

/* ---------- current clock ---------- */

/* CallNtPowerInformation(ProcessorInformation) returns per-processor
   CurrentMhz in a single call. The PDH "Processor Frequency" counter needs a
   two-sample window whose first PdhCollectQueryData costs well over 100 ms
   on a cold system, which is far too slow for the live loop. */
static unsigned int current_clock_from_power_information(void)
{
    SYSTEM_INFO si;
    HF_PROCESSOR_POWER_INFORMATION* ppi;
    unsigned int best = 0;

    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors == 0) return 0;

    ppi = (HF_PROCESSOR_POWER_INFORMATION*)calloc(si.dwNumberOfProcessors, sizeof(*ppi));
    if (!ppi) return 0;

    if (CallNtPowerInformation(ProcessorInformation, NULL, 0, ppi,
            (ULONG)(si.dwNumberOfProcessors * sizeof(*ppi))) == 0) {
        for (DWORD i = 0; i < si.dwNumberOfProcessors; i++)
            if (ppi[i].CurrentMhz > best) best = ppi[i].CurrentMhz;
    }
    free(ppi);
    return best;
}

/* Registry ~MHz is the base clock and is always present. */
static unsigned int base_clock_from_registry(void)
{
    HKEY hKey;
    DWORD mhz = 0, size = sizeof(mhz), type = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;
    if (RegQueryValueExA(hKey, "~MHz", NULL, &type, (LPBYTE)&mhz, &size) != ERROR_SUCCESS)
        mhz = 0;
    RegCloseKey(hKey);
    return type == REG_DWORD ? mhz : 0;
}

static unsigned int current_clock_mhz(void)
{
    unsigned int live = current_clock_from_power_information();
    if (live > 0) {
        debug_log("clock source=CallNtPowerInformation value=%u MHz", live);
        return live;
    }
    {
        unsigned int base = base_clock_from_registry();
        debug_log("clock source=registry ~MHz value=%u MHz (no live reading)", base);
        return base;
    }
}

void cpu_collect_live(hf_state* s)
{
    unsigned int mhz = current_clock_mhz();
    if (mhz == 0)
        debug_log("PDH Processor Frequency unavailable, clock unknown");
    else
        debug_log("current clock = %u MHz", mhz);
    s->cpu_freq_mhz = mhz;
}