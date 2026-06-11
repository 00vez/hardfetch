#include "uptime.h"
#include "output.h"

#include <stdio.h>
#include <windows.h>

void print_uptime_info(void)
{
    ULONGLONG ms = GetTickCount64();
    ULONGLONG totalSeconds = ms / 1000;
    ULONGLONG minutes = (totalSeconds / 60) % 60;
    ULONGLONG hours = (totalSeconds / 3600) % 24;
    ULONGLONG days = totalSeconds / 86400;

    char buf[128];
    if (days > 0)
        snprintf(buf, sizeof(buf), "%llu days, %llu hours, %llu mins", days, hours, minutes);
    else if (hours > 0)
        snprintf(buf, sizeof(buf), "%llu hours, %llu mins", hours, minutes);
    else
        snprintf(buf, sizeof(buf), "%llu mins", minutes > 0 ? minutes : 1);
    print_block("Uptime", buf);
}
