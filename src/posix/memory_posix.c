#include "../memory.h"
#include "../memory_spd.h"
#include "../output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

static unsigned long kb_from_line(const char* line)
{
    while (*line && (*line < '0' || *line > '9')) line++;
    if (!*line) return 0;
    return strtoul(line, NULL, 10);
}

void print_memory_info(void)
{
    double totalGiB = 0, usedGiB = 0;

#if defined(__APPLE__)
    uint64_t phys = 0;
    size_t len = sizeof(phys);
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    if (sysctl(mib, 2, &phys, &len, NULL, 0) == 0) {
        totalGiB = phys / (1024.0 * 1024.0 * 1024.0);

        mach_host_t host = mach_host_self();
        vm_statistics64_data_t vs;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(host, HOST_VM_INFO64,
                              (host_info64_t)&vs, &count) == KERN_SUCCESS) {
            uint64_t page_size = 0;
            host_page_size(host, &page_size);
            uint64_t freeb  = vs.free_count * page_size;
            uint64_t inact  = vs.inactive_count * page_size;
            double availGiB = (double)(freeb + inact) / (1024.0*1024.0*1024.0);
            usedGiB = totalGiB - availGiB;
            if (usedGiB < 0) usedGiB = 0;
        }
        mach_port_deallocate(mach_task_self(), host);
    }
#else
    FILE* f = fopen("/proc/meminfo", "r");
    if (f) {
        unsigned long total = 0, avail = 0;
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0)
                total = kb_from_line(line);
            else if (strncmp(line, "MemAvailable:", 13) == 0) {
                avail = kb_from_line(line);
                break;
            }
        }
        fclose(f);
        if (total > 0) {
            totalGiB = (double)total / 1024.0 / 1024.0;
            usedGiB  = (avail ? (double)(total - avail) : total) / 1024.0 / 1024.0;
        }
    }
#endif

    if (totalGiB <= 0) { print_block("RAM", "N/A"); return; }

    char buf[160];
    int pos = snprintf(buf, sizeof(buf), "%.1f / %.1f GiB", usedGiB, totalGiB);

    unsigned int speed = 0, cas = 0;
    if (get_memory_speed(&speed, &cas) == 0 && speed > 0) {
        if (cas > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  %u MHz  CL%u", speed, cas);
        else        pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  %u MHz", speed);
    }
    print_block("RAM", buf);
}
