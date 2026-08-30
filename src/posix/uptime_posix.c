#include "../uptime.h"
#include "../output.h"

#include <stdio.h>
#include <time.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

void print_uptime_info(void)
{
    double up = 0;
#if defined(__APPLE__)
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0) {
        time_t now = time(NULL);
        up = (double)(now - boottime.tv_sec);
    }
#else
    FILE* f = fopen("/proc/uptime", "r");
    if (f) {
        if (fscanf(f, "%lf", &up) != 1) up = 0;
        fclose(f);
    }
#endif
    if (up <= 0) { print_block("Uptime", "N/A"); return; }

    long s = (long)up;
    long days = s / 86400; s %= 86400;
    long hrs  = s / 3600;  s %= 3600;
    long mins = s / 60;

    char buf[96];
    if (days > 0)
        snprintf(buf, sizeof(buf), "%ld days, %ld hours, %ld mins", days, hrs, mins);
    else
        snprintf(buf, sizeof(buf), "%ld hours, %ld mins", hrs, mins);
    print_block("Uptime", buf);
}
