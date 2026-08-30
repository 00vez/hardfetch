#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>
#if defined(__APPLE__)
#include "apple_smc.h"
#endif

void print_cpu_temp_power(double load)
{
    char detail[128];
    int tempC = -1;
#if defined(__APPLE__)
    {
        int val = 0;
        const char* keys[] = {"TC0D", "TC0P", "TC0E", "Tp05", "Tp09", "Tp0D", "Tp0b", "Tp01", NULL};
        for (int i = 0; keys[i]; i++) {
            if (apple_smc_read_temp(keys[i], &val) == 0 && val > 0) {
                tempC = val;
                break;
            }
        }
    }
#else
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int milli = 0;
        if (fscanf(f, "%d", &milli) == 1) tempC = milli / 1000;
        fclose(f);
    }
#endif

    int pos = snprintf(detail, sizeof(detail), "Load  %.0f%%", load);
    if (tempC >= 0)
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  %d\xc2\xb0""C", tempC);
    else
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  N/A");
    print_detail(detail);
}
