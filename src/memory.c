#include "memory.h"
#include "memory_spd.h"
#include "output.h"

#include <stdio.h>
#include <windows.h>

void print_memory_info(void)
{
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);

    if (GlobalMemoryStatusEx(&statex)) {
        double usedGiB = (statex.ullTotalPhys - statex.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
        double totalGiB = statex.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);

        char buf[128];
        int pos = snprintf(buf, sizeof(buf), "%.1f / %.1f GiB", usedGiB, totalGiB);

        unsigned int speed = 0, cas = 0;
        if (get_memory_speed(&speed, &cas) == 0 && speed > 0) {
            if (cas > 0)
                pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  %u MHz  CL%u", speed, cas);
            else
                pos += snprintf(buf + pos, sizeof(buf) - pos, "  |  %u MHz", speed);
        }

        print_block("RAM", buf);
    }
}
