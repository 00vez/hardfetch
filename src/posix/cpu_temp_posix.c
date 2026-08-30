#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include "apple_smc.h"
#endif

static int is_arm64(void) {
#if defined(__APPLE__)
    int v = 0; size_t l = sizeof(v);
    if (sysctlbyname("hw.optional.arm64", &v, &l, NULL, 0) == 0) return v;
#endif
    return 0;
}

void print_cpu_temp_power(double load)
{
    char detail[128];
    int tempC = -1;
#if defined(__APPLE__)
    if (is_arm64()) {
        apple_smc_cpu_temp(&tempC);
    } else {
        const char *keys[] = {"TC0D","TC0P",NULL};
        for (int i=0; keys[i]; i++) { int v; if (apple_smc_read_temp(keys[i],&v)==0){tempC=v;break;} }
    }
#else
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int milli = 0;
        if (fscanf(f, "%d", &milli) == 1) tempC = milli / 1000;
        fclose(f);
    }
#endif

    int pos;
    if (load >= 0) pos = snprintf(detail, sizeof(detail), "Load  %.1f%%", load);
    else pos = snprintf(detail, sizeof(detail), "Load  N/A");
    if (tempC >= 0)
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  %d\xc2\xb0""C", tempC);
    else
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  N/A");
    print_detail(detail);
}
