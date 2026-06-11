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
    PDH_HQUERY pdhQuery;
    if (PdhOpenQueryA(NULL, 0, &pdhQuery) == ERROR_SUCCESS) {
        PDH_HCOUNTER freqCounter;
        if (PdhAddCounterA(pdhQuery, "\\Processor Performance(PPM_Processor_0)\\Processor Frequency", 0, &freqCounter) == ERROR_SUCCESS ||
            PdhAddCounterA(pdhQuery, "\\Processor Information(_Total)\\Processor Frequency", 0, &freqCounter) == ERROR_SUCCESS) {
            PdhCollectQueryData(pdhQuery);
            Sleep(50);
            PdhCollectQueryData(pdhQuery);
            PDH_FMT_COUNTERVALUE val;
            if (PdhGetFormattedCounterValue(freqCounter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, NULL, &val) == ERROR_SUCCESS && val.CStatus == 0)
                freqMhz = (unsigned int)(val.doubleValue + 0.5);
        }

        PDH_HCOUNTER pctCounter;
        if (PdhAddCounterA(pdhQuery, "\\Processor Performance(PPM_Processor_0)\\% of Maximum Frequency", 0, &pctCounter) == ERROR_SUCCESS) {
            PdhCollectQueryData(pdhQuery);
            Sleep(50);
            PdhCollectQueryData(pdhQuery);
            PDH_FMT_COUNTERVALUE val;
            if (PdhGetFormattedCounterValue(pctCounter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, NULL, &val) == ERROR_SUCCESS && val.CStatus == 0 && val.doubleValue > 0) {
                double pct = val.doubleValue;
                if (pct < 99.5 && pct > 0) {
                    unsigned int boostMhz = (unsigned int)(freqMhz / (pct / 100.0) + 0.5);
                    if (boostMhz > freqMhz) freqMhz = boostMhz;
                }
            }
        }
        PdhCloseQuery(pdhQuery);
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
        Sleep(100);
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
