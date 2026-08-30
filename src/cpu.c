#include "cpu.h"
#include "cpu_temp.h"
#include "output.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <pdh.h>

static void rtrim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static volatile unsigned int g_pdh_freq = 0;
static HANDLE g_pdh_evt = NULL;

static DWORD WINAPI pdh_worker(LPVOID arg)
{
    unsigned int base = (unsigned int)(uintptr_t)arg;
    unsigned int f = base;
    PDH_HQUERY q;
    if (PdhOpenQueryA(NULL, 0, &q) == ERROR_SUCCESS) {
        PDH_HCOUNTER fc = NULL, pc = NULL;
        int hf = 0, hp = 0;
        if (PdhAddCounterA(q, "\\Processor Performance(PPM_Processor_0)\\Processor Frequency", 0, &fc) == ERROR_SUCCESS ||
            PdhAddCounterA(q, "\\Processor Information(_Total)\\Processor Frequency", 0, &fc) == ERROR_SUCCESS)
            hf = 1;
        if (PdhAddCounterA(q, "\\Processor Performance(PPM_Processor_0)\\% of Maximum Frequency", 0, &pc) == ERROR_SUCCESS)
            hp = 1;
        if (hf || hp) {
            PdhCollectQueryData(q);
            Sleep(20);
            PdhCollectQueryData(q);
            if (hf) {
                PDH_FMT_COUNTERVALUE v;
                if (PdhGetFormattedCounterValue(fc, PDH_FMT_DOUBLE|PDH_FMT_NOCAP100, NULL, &v) == ERROR_SUCCESS && v.CStatus == 0)
                    f = (unsigned int)(v.doubleValue + 0.5);
            }
            if (hp) {
                PDH_FMT_COUNTERVALUE v;
                if (PdhGetFormattedCounterValue(pc, PDH_FMT_DOUBLE|PDH_FMT_NOCAP100, NULL, &v) == ERROR_SUCCESS && v.CStatus == 0 && v.doubleValue > 0) {
                    double pct = v.doubleValue;
                    if (pct < 99.5 && pct > 0) {
                        unsigned int b = (unsigned int)(f / (pct/100.0) + 0.5);
                        if (b > f) f = b;
                    }
                }
            }
        }
        PdhCloseQuery(q);
    }
    g_pdh_freq = f;
    if (g_pdh_evt) SetEvent(g_pdh_evt);
    return 0;
}

void print_cpu_info(void)
{
    char name[256] = {0};
    DWORD mhz = 0;
    DWORD numLogical = 0;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(name);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)name, &size) != ERROR_SUCCESS)
            name[0] = '\0';
        rtrim(name);

        size = sizeof(mhz);
        RegQueryValueExA(hKey, "~MHz", NULL, NULL, (LPBYTE)&mhz, &size);

        RegCloseKey(hKey);
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    numLogical = si.dwNumberOfProcessors;

    unsigned int freqMhz = mhz;
    g_pdh_evt = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (g_pdh_evt) {
        HANDLE t = CreateThread(NULL, 0, pdh_worker, (LPVOID)(uintptr_t)mhz, 0, NULL);
        if (t) {
            if (WaitForSingleObject(g_pdh_evt, 100) == WAIT_OBJECT_0)
                freqMhz = g_pdh_freq;
            CloseHandle(t);
        }
        CloseHandle(g_pdh_evt);
        g_pdh_evt = NULL;
    }

    // Strip " N-Core Processor" / " N-Core" suffix from name
    char shortName[256];
    strcpy_s(shortName, sizeof(shortName), name);
    char* suffix = strstr(shortName, "-Core");
    if (suffix) {
        char* p = suffix - 1;
        while (p > shortName && *p >= '0' && *p <= '9') p--;
        if (*p == ' ') *p = '\0';
    }

    // Build main line
    char mainLine[300];
    int pos = 0;
    if (shortName[0])
        pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, "%s", shortName);
    else
        pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, "Unknown CPU");
    if (numLogical > 0)
        pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " (%lu)", (unsigned long)numLogical);
    if (freqMhz > 0)
        pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " @ %.2f GHz", freqMhz / 1000.0);

    print_block_green("CPU", mainLine);

    // Detail line: Load + Temp (no Power)
    double load = 0.0;
    FILETIME idleTime1, kernelTime1, userTime1;
    if (GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) {
        Sleep(20);
        FILETIME idleTime2, kernelTime2, userTime2;
        if (GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) {
            ULARGE_INTEGER idle1, kernel1, user1;
            ULARGE_INTEGER idle2, kernel2, user2;
            idle1.LowPart = idleTime1.dwLowDateTime; idle1.HighPart = idleTime1.dwHighDateTime;
            kernel1.LowPart = kernelTime1.dwLowDateTime; kernel1.HighPart = kernelTime1.dwHighDateTime;
            user1.LowPart = userTime1.dwLowDateTime; user1.HighPart = userTime1.dwHighDateTime;
            idle2.LowPart = idleTime2.dwLowDateTime; idle2.HighPart = idleTime2.dwHighDateTime;
            kernel2.LowPart = kernelTime2.dwLowDateTime; kernel2.HighPart = kernelTime2.dwHighDateTime;
            user2.LowPart = userTime2.dwLowDateTime; user2.HighPart = userTime2.dwHighDateTime;

            ULONGLONG idleDiff = idle2.QuadPart - idle1.QuadPart;
            ULONGLONG kernelDiff = kernel2.QuadPart - kernel1.QuadPart;
            ULONGLONG userDiff = user2.QuadPart - user1.QuadPart;
            ULONGLONG totalDiff = kernelDiff + userDiff;

            if (totalDiff > 0)
                load = (1.0 - (double)idleDiff / totalDiff) * 100.0;
        }
    }

    print_cpu_temp_power(load);
}
