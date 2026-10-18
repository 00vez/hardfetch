#include "../uptime.h"
#include "../debug.h"

#include <stdio.h>
#include <time.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

void uptime_collect(hf_state* s)
{
    double up = 0;

    debug_begin("uptime");

#if defined(__APPLE__)
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };

    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0)
        up = (double)(time(NULL) - boottime.tv_sec);
#else
    {
        FILE* f = fopen("/proc/uptime", "r");
        if (f) {
            if (fscanf(f, "%lf", &up) != 1) up = 0;
            fclose(f);
        }
    }
#endif

    if (up <= 0) up = 0;
    s->uptime_secs = (unsigned long long)up;
    s->uptime_secs_live = s->uptime_secs;

    debug_log("uptime %llu s", s->uptime_secs);
    debug_end();
}
