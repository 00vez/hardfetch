#include "../memory.h"
#include "../debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
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

/* Speed and CAS come from dmidecode on Linux and hw.memsize on macOS; neither
   is readable without privileges in most default setups, so the fields stay
   empty rather than being guessed. */
void memory_collect_spd(hf_state* s)
{
    debug_begin("ram-spd");

    s->ram_mhz = 0;
    s->ram_cl = 0;
    s->ram_type[0] = 0;
    s->ram_modules = 0;
    s->ram_module_bytes = 0;

#if defined(__APPLE__)
    {
        /* Apple Silicon has no SPD data reachable without a kernel driver. */
        debug_log("no SPD source on this platform");
    }
#else
    {
        unsigned long long total = 0;
        FILE* f = fopen("/proc/meminfo", "r");

        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "MemTotal:", 9) == 0) {
                    unsigned long kb = kb_from_line(line);
                    if (kb > 0) total = (unsigned long long)kb * 1024ULL;
                    break;
                }
            }
            fclose(f);
        }
        if (total > 0) {
            s->ram_modules = 1;
            s->ram_module_bytes = total;
        }
        debug_log("modules=%u bytes=%llu (speed and CAS need privileged SPD access)",
                  s->ram_modules, s->ram_module_bytes);
    }
#endif

    debug_end();
}
