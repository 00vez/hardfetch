#define _GNU_SOURCE
#include "../cpu.h"
#include "../cpu_temp.h"
#include "../debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#include <unistd.h>
#include "apple_pmgr.h"

static double cpu_usage_percent(unsigned interval_ms)
{
    natural_t n1 = 0, n2 = 0;
    processor_info_array_t t1 = NULL, t2 = NULL;
    mach_msg_type_number_t c1 = 0, c2 = 0;
    uint64_t b1 = 0, tot1 = 0, b2 = 0, tot2 = 0;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                            &n1, &t1, &c1) != KERN_SUCCESS) return -1;
    usleep(interval_ms * 1000);
    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                            &n2, &t2, &c2) != KERN_SUCCESS) {
        vm_deallocate(mach_task_self(), (vm_address_t)t1, c1 * sizeof(integer_t));
        return -1;
    }
    for (natural_t i = 0; i < n1 && i < n2; i++) {
        integer_t* p1 = t1 + i * CPU_STATE_MAX;
        integer_t* p2 = t2 + i * CPU_STATE_MAX;
        uint64_t bb1 = p1[CPU_STATE_USER] + p1[CPU_STATE_SYSTEM] + p1[CPU_STATE_NICE];
        uint64_t tt1 = bb1 + p1[CPU_STATE_IDLE];
        uint64_t bb2 = p2[CPU_STATE_USER] + p2[CPU_STATE_SYSTEM] + p2[CPU_STATE_NICE];
        uint64_t tt2 = bb2 + p2[CPU_STATE_IDLE];
        b1 += bb1; tot1 += tt1; b2 += bb2; tot2 += tt2;
    }
    vm_deallocate(mach_task_self(), (vm_address_t)t1, c1 * sizeof(integer_t));
    vm_deallocate(mach_task_self(), (vm_address_t)t2, c2 * sizeof(integer_t));
    {
        uint64_t dt = tot2 - tot1, db = b2 - b1;
        return dt ? (double)db / (double)dt : -1;
    }
}
#endif

static void trim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = 0;
}

static void parse_after(const char* line, const char* key, char* out, size_t n)
{
    const char* p;
    out[0] = 0;
    p = strstr(line, key);
    if (!p) return;
    p += strlen(key);
    while (*p == ':' || *p == ' ' || *p == '\t') p++;
    snprintf(out, n, "%s", p);
    trim(out);
}

#ifndef __APPLE__
/* Samples the aggregate line of /proc/stat. Returns the deltas against the
   previous sample through the two out parameters. */
static int read_cpu_ticks(unsigned long long* dTotal, unsigned long long* dBusy)
{
    static unsigned long long prevTotal = 0, prevIdle = 0;
    static int first = 1;
    unsigned long long user = 0, nice = 0, sys = 0, idle = 0, total = 0;
    char line[512];
    FILE* f;

    *dTotal = 0;
    *dBusy = 0;

    f = fopen("/proc/stat", "r");
    if (!f) return 0;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);
    if (sscanf(line, "cpu %llu %llu %llu %llu", &user, &nice, &sys, &idle) != 4)
        return 0;

    total = user + nice + sys + idle;
    if (first) {
        prevTotal = total;
        prevIdle = idle;
        first = 0;
        return 0;
    }
    if (total <= prevTotal) return 0;

    *dTotal = total - prevTotal;
    {
        unsigned long long dIdle = (idle > prevIdle) ? idle - prevIdle : 0;
        *dBusy = (*dTotal > dIdle) ? *dTotal - dIdle : 0;
    }
    prevTotal = total;
    prevIdle = idle;
    return 1;
}

static double read_load(void)
{
    unsigned long long dTotal = 0, dBusy = 0;
    if (!read_cpu_ticks(&dTotal, &dBusy) || dTotal == 0) return 0;
    return (double)dBusy / (double)dTotal;
}
#endif

/* The load window spans the rest of the collection, the same way it does on
   Windows, so sampling it costs no wall clock of its own. */
static double s_t0 = -1;

void cpu_sample_begin(void)
{
#if defined(__APPLE__)
    (void)cpu_usage_percent(0);
    s_t0 = -1;
#else
    read_load();
    s_t0 = 0;
#endif
}

/* Below this many jiffies the ratio is pure noise and flips between 0 and
   100 percent. Three jiffies is 30 ms of CPU time, enough for a usable
   reading; below that a short extra sample is taken once. The earlier floor
   of ten made a two thread VM sleep on every single run. */
#define MIN_TICKS 3

void cpu_sample_end(hf_state* s)
{
    debug_begin("load");

    s->cpu_load = 0.0;

#if defined(__APPLE__)
    {
        double l = cpu_usage_percent(120);
        if (l > 0) s->cpu_load = l;
        debug_log("load %.4f", s->cpu_load);
    }
#else
    {
        unsigned long long dTotal = 0, dBusy = 0;

        if (!read_cpu_ticks(&dTotal, &dBusy) || dTotal == 0) {
            debug_log("no usable delta in the collection window");
        } else if (dTotal < MIN_TICKS) {
            /* Too short to trust. Take a proper reading instead of
               publishing a 0 or 100 percent artefact. */
            struct timespec ts = { 0, 60 * 1000 * 1000 };
            unsigned long long t2 = 0, b2 = 0;

            nanosleep(&ts, NULL);
            if (read_cpu_ticks(&t2, &b2) && t2 > 0) {
                dTotal += t2;
                dBusy += b2;
                s->cpu_load = (double)dBusy / (double)dTotal;
                debug_log("window was %llu ticks, extended to %llu -> %.4f",
                          dTotal - t2, dTotal, s->cpu_load);
            } else {
                debug_log("window %llu ticks below the floor and no second sample",
                          dTotal);
            }
        } else {
            s->cpu_load = (double)dBusy / (double)dTotal;
            debug_log("window %llu ticks, busy %llu -> %.4f", dTotal, dBusy, s->cpu_load);
        }
    }
#endif

    debug_end();
}

void cpu_collect_live(hf_state* s)
{
    double mhz = 0;

    s->cpu_freq_mhz = 0;

#if defined(__APPLE__)
    {
        uint32_t p_mhz = appleMaxFreqMHz("voltage-states5-sram");
        if (p_mhz > 0) mhz = (double)p_mhz;
    }
#else
    {
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                char val[64];
                parse_after(line, "cpu MHz", val, sizeof(val));
                if (val[0]) { mhz = strtod(val, NULL); break; }
            }
            fclose(f);
        }
        if (mhz <= 0) {
            /* Apple silicon and some VMs do not report a live clock. */
            f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
            if (f) {
                if (fscanf(f, "%lf", &mhz) != 1) mhz = 0;
                else mhz /= 1000.0;
                fclose(f);
            }
        }
    }
#endif

    if (mhz > 0) s->cpu_freq_mhz = (unsigned int)(mhz + 0.5);
    debug_log("clock = %u MHz", s->cpu_freq_mhz);
}

void cpu_collect_static(hf_state* s)
{
    int logical = 0;
    int physical = 0;

    debug_begin("cpu");
    snprintf(s->cpu_name, sizeof(s->cpu_name), "%s", "Unknown CPU");
    s->cpu_boost_mhz = 0;
    s->cpu_cores = 0;
    s->cpu_threads = 0;

#if defined(__APPLE__)
    {
        char brand[256] = {0};
        size_t len = sizeof(brand);
        int ncpu = 0;

        if (sysctlbyname("machdep.cpu.brand_string", brand, &len, NULL, 0) == 0)
            snprintf(s->cpu_name, sizeof(s->cpu_name), "%.*s",
                     (int)sizeof(s->cpu_name) - 1, brand);
        len = sizeof(ncpu);
        if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) == 0)
            logical = ncpu;
        len = sizeof(ncpu);
        if (sysctlbyname("hw.physicalcpu", &ncpu, &len, NULL, 0) == 0)
            physical = ncpu;
        {
            uint32_t p_mhz = appleMaxFreqMHz("voltage-states5-sram");
            if (p_mhz > 0) s->cpu_boost_mhz = p_mhz;
        }
    }
#else
    {
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[512];
            int got_name = 0;
            char cores[64] = {0};

            while (fgets(line, sizeof(line), f)) {
                char val[256];
                if (strncmp(line, "processor", 9) == 0) logical++;
                if (!got_name) {
                    parse_after(line, "model name", val, sizeof(val));
                    if (val[0]) {
                        snprintf(s->cpu_name, sizeof(s->cpu_name), "%.*s",
                                 (int)sizeof(s->cpu_name) - 1, val);
                        got_name = 1;
                    }
                }
                if (!cores[0]) {
                    parse_after(line, "cpu cores", val, sizeof(val));
                    if (val[0])
                        snprintf(cores, sizeof(cores), "%.*s",
                                 (int)sizeof(cores) - 1, val);
                }
            }
            fclose(f);
            if (cores[0]) physical = atoi(cores);
        }
        if (physical <= 0 && logical > 0) physical = logical;
        if (logical <= 0) {
            long n = sysconf(_SC_NPROCESSORS_ONLN);
            if (n > 0) logical = (int)n;
        }
    }
#endif

    /* /proc/cpuinfo appends "8-Core Processor" or "@ 3.40GHz" to the model
       name, and sysctl appends the core count. The Windows build shows the
       bare model, so both are stripped here to keep the two lines equal. */
    {
        char* suffix = strstr(s->cpu_name, "-Core");
        if (suffix) {
            char* bp = suffix - 1;
            while (bp > s->cpu_name && *bp >= '0' && *bp <= '9') bp--;
            if (bp > s->cpu_name && *bp == ' ') *bp = 0;
            else if (bp == s->cpu_name) s->cpu_name[0] = 0;
        }
    }
    {
        char* at = strstr(s->cpu_name, " @ ");
        if (at) *at = 0;
    }
    trim(s->cpu_name);

    s->cpu_threads = logical;
    s->cpu_cores = physical;

    cpu_collect_live(s);
    debug_log("name='%s' %dC/%dT clock=%u boost=%u",
              s->cpu_name, s->cpu_cores, s->cpu_threads,
              s->cpu_freq_mhz, s->cpu_boost_mhz);
    debug_end();
}
