#define _POSIX_C_SOURCE 200809L
#include "../memory.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifndef __APPLE__
static unsigned long kb_from_line(const char* line)
{
    while (*line && (*line < '0' || *line > '9')) line++;
    return *line ? strtoul(line, NULL, 10) : 0;
}
#endif

void memory_collect_usage(hf_state* s)
{
    unsigned long long total = 0, avail = 0;

    debug_begin("ram");

#if defined(__APPLE__)
    {
        uint64_t phys = 0;
        size_t len = sizeof(phys);
        int mib[2] = { CTL_HW, HW_MEMSIZE };

        if (sysctl(mib, 2, &phys, &len, NULL, 0) == 0)
            total = (unsigned long long)phys;

        {
            mach_port_t host = mach_host_self();
            vm_statistics64_data_t vs;
            mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

            memset(&vs, 0, sizeof(vs));
            if (host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vs,
                                  &count) == KERN_SUCCESS) {
                vm_size_t page = 0;
                host_page_size(host, &page);
                avail = (unsigned long long)(vs.free_count + vs.inactive_count) * page;
            }
            mach_port_deallocate(mach_task_self(), host);
        }
    }
#else
    {
        FILE* f = fopen("/proc/meminfo", "r");
        if (f) {
            char line[256];
            unsigned long kt = 0, ka = 0;
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "MemTotal:", 9) == 0) kt = kb_from_line(line);
                else if (strncmp(line, "MemAvailable:", 13) == 0) { ka = kb_from_line(line); break; }
            }
            fclose(f);
            total = (unsigned long long)kt * 1024ULL;
            avail = (unsigned long long)ka * 1024ULL;
        }
    }
#endif

    if (avail > total) avail = total;
    s->ram_total = total;
    s->ram_used = total - avail;

    debug_log("total=%llu used=%llu avail=%llu", s->ram_total, s->ram_used, avail);
    debug_end();
}

/* ---------------- SPD ----------------
   dmidecode needs privileges on most distributions. When it is missing or
   refuses, the fields stay empty, which is what the Windows build does when
   the firmware does not report them. Nothing is inferred from the clock. */

#ifndef __APPLE__

static void rstrip(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' '  || s[n - 1] == '\t'))
        s[--n] = 0;
}

static unsigned int first_number(const char* s)
{
    char* end = NULL;
    unsigned long v;

    while (*s && (*s < '0' || *s > '9')) s++;
    if (!*s) return 0;
    v = strtoul(s, &end, 10);
    return (end && end != s) ? (unsigned int)v : 0;
}

/* "6000 MT/s" gives 6000; "Unknown" and "Not Specified" give nothing. */
static unsigned int speed_from(const char* v)
{
    if (!v) return 0;
    if (strstr(v, "Unknown") || strstr(v, "Not Specified")) return 0;
    return first_number(v);
}

/* Longest label first, so DDR5 is never read as DDR. */
static const char* type_from(const char* v)
{
    if (!v) return 0;
    if (strstr(v, "DDR5"))   return "DDR5";
    if (strstr(v, "DDR4"))   return "DDR4";
    if (strstr(v, "DDR3"))   return "DDR3";
    if (strstr(v, "DDR2"))   return "DDR2";
    if (strstr(v, "LPDDR5")) return "LPDDR5";
    if (strstr(v, "LPDDR4")) return "LPDDR4";
    if (strstr(v, "DDR"))    return "DDR";
    return 0;
}

/* dmidecode prints "32 GB" for normal modules and a bare megabyte count for
   the rare large one. */
static unsigned long long size_bytes_from(const char* v)
{
    unsigned long long number;

    if (!v) return 0;
    while (*v && (*v < '0' || *v > '9')) v++;
    if (!*v) return 0;
    number = strtoull(v, NULL, 10);
    if (number == 0) return 0;

    if (strstr(v, "TB")) return number * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    if (strstr(v, "GB")) return number * 1024ULL * 1024ULL * 1024ULL;
    if (strstr(v, "MB")) return number * 1024ULL * 1024ULL;
    if (strstr(v, "KB")) return number * 1024ULL;
    return number * 1024ULL * 1024ULL;
}

static void read_spd(hf_state* s)
{
    FILE* p = popen("dmidecode -t 17 2>/dev/null", "r");
    char line[512];
    int inBlock = 0;
    int haveConfigured = 0;
    unsigned int blocks = 0;
    unsigned long long biggest = 0;

    if (!p) {
        debug_log("dmidecode not available, RAM speed stays empty");
        return;
    }

    while (fgets(line, sizeof(line), p)) {
        char* colon;
        char key[512];
        const char* val;

        rstrip(line);
        if (line[0] == 0) continue;

        if (strstr(line, "Memory Device")) {
            inBlock = 1;
            blocks++;
            continue;
        }
        if (line[0] != '\t' && line[0] != ' ') continue;
        if (!inBlock) continue;

        colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;

        snprintf(key, sizeof(key), "%.*s",
                 (int)sizeof(key) - 1, line + 1);
        rstrip(key);

        val = colon + 1;
        while (*val == ' ' || *val == '\t') val++;

        if (!strcmp(key, "Type")) {
            const char* t = type_from(val);
            if (t && !s->ram_type[0])
                snprintf(s->ram_type, sizeof(s->ram_type), "%s", t);
        } else if (!strcmp(key, "Configured Memory Speed")) {
            /* The configured clock wins over the module maximum, which is a
               different number and appears earlier in the block. */
            unsigned int mhz = speed_from(val);
            if (mhz > 0) { s->ram_mhz = mhz; haveConfigured = 1; }
        } else if (!strcmp(key, "Speed")) {
            unsigned int mhz = speed_from(val);
            if (mhz > 0 && !haveConfigured) s->ram_mhz = mhz;
        } else if (!strcmp(key, "Configured CAS Latency")) {
            unsigned int cl = first_number(val);
            if (cl > 0 && s->ram_cl == 0) s->ram_cl = cl;
        } else if (!strcmp(key, "Size")) {
            unsigned long long b = size_bytes_from(val);
            if (b > biggest) biggest = b;
        }
    }
    pclose(p);

    if (s->ram_mhz > 0 || s->ram_cl > 0 || s->ram_type[0] || biggest > 0) {
        s->ram_modules = blocks;
        s->ram_module_bytes = biggest;
    }

    debug_log("blocks=%u mhz=%u cl=%u type='%s' module=%llu bytes",
              s->ram_modules, s->ram_mhz, s->ram_cl, s->ram_type,
              s->ram_module_bytes);
}

#endif /* !__APPLE__ */

void memory_collect_spd(hf_state* s)
{
    debug_begin("ram-spd");

    s->ram_mhz = 0;
    s->ram_cl = 0;
    s->ram_type[0] = 0;
    s->ram_modules = 0;
    s->ram_module_bytes = 0;

#if defined(__APPLE__)
    /* Apple Silicon exposes no SPD data without a kernel driver. */
    debug_log("no SPD source on this platform");
#else
    read_spd(s);
#endif

    debug_end();
}
