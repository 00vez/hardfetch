#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>

void print_cpu_temp_power(double load)
{
    char detail[128];
    int tempC = -1;
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int milli = 0;
        if (fscanf(f, "%d", &milli) == 1) tempC = milli / 1000;
        fclose(f);
    }

    int pos = snprintf(detail, sizeof(detail), "Load  %.0f%%", load);
    if (tempC >= 0)
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  %d\xc2\xb0""C", tempC);
    else
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  N/A");
    print_detail(detail);
}
