#define _GNU_SOURCE
#include "../cpu.h"
#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if defined(__APPLE__)
#if defined(__APPLE__)
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#include "apple_pmgr.h"
#endif

static void trim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t'))
        s[--n] = '\0';
}

static void parse_after(const char* line, const char* key, char* out, size_t n)
{
    out[0] = '\0';
    const char* p = strstr(line, key);
    if (!p) return;
    p += strlen(key);
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    strncpy(out, p, n - 1);
    out[n - 1] = '\0';
    trim(out);
}

static double read_load(void)
{
    static long long prev_idle, prev_total;
    static int first = 1;
    double load = 0;

    FILE* f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char line[512];
    if (fgets(line, sizeof(line), f)) {
        long long user=0, nice=0, sys=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
        sscanf(line, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
               &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
        long long total = user + nice + sys + idle + iowait + irq + softirq + steal;
        long long idl   = idle + iowait;
        if (!first) {
            long long dtotal = total - prev_total;
            long long didle  = idl   - prev_idle;
            if (dtotal > 0) load = (1.0 - (double)didle / (double)dtotal) * 100.0;
        }
        prev_total = total; prev_idle = idl; first = 0;
    }
    fclose(f);
    return load;
}

#if defined(__APPLE__)
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

void print_cpu_info(void)
{
    char name[256] = "Unknown CPU";
    double mhz = 0;
    int logical = 0;

#if defined(__APPLE__)
    {
        int got_name = 0;
        (void)got_name;
        char brand[256] = {0};
        size_t len = sizeof(brand);
        if (sysctlbyname("machdep.cpu.brand_string", brand, &len, NULL, 0) == 0) {
            strncpy(name, brand, sizeof(name)-1); name[sizeof(name)-1]='\0';
            got_name = 1;
        }
        int ncpu = 0;
        len = sizeof(ncpu);
        sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);
        logical = ncpu;
        uint32_t mhz_max = appleMaxFreqMHz("voltage-states5-sram");
        if (mhz_max == 0) mhz_max = appleMaxFreqMHz("voltage-states1-sram");
        mhz = (double)mhz_max;
    }
#else
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        int got_name = 0;
        while (fgets(line, sizeof(line), f)) {
            char val[256];
            if (!got_name) { parse_after(line, "model name", val, sizeof(val)); if (val[0]) { strncpy(name, val, sizeof(name)-1); name[sizeof(name)-1]='\0'; got_name = 1; } }
            parse_after(line, "cpu MHz", val, sizeof(val));
            if (val[0]) mhz = strtod(val, NULL);
            if (strncmp(line, "processor", 9) == 0) logical++;
        }
        fclose(f);
    }
#endif

    char* suffix = strstr(name, "-Core");
    if (suffix) {
        char* bp = suffix - 1;
        while (bp > name && *bp >= '0' && *bp <= '9') bp--;
        if (*bp == ' ') *bp = '\0';
    }

    (void)parse_after;
    double s1 = read_load();
    struct timespec ts = { 0, 20 * 1000 * 1000 };
    nanosleep(&ts, NULL);
    double s2 = read_load();
    (void)s1;
    double load = s2;

    char mainLine[320];
    int pos = snprintf(mainLine, sizeof(mainLine), "%s (%d)", name, logical);
    if (mhz > 0) pos += snprintf(mainLine + pos, sizeof(mainLine) - pos, " @ %.2f GHz", mhz / 1000.0);
    print_block_green("CPU", mainLine);

    print_cpu_temp_power(load);
}
