#include "../storage.h"
#include "../output.h"

#include <stdio.h>
#include <sys/statvfs.h>

void print_storage_info(void)
{
    struct statvfs v;
    if (statvfs("/", &v) != 0) { print_block("Disk", "N/A"); return; }

    unsigned long long total = (unsigned long long)v.f_blocks * v.f_frsize;
    unsigned long long freeb = (unsigned long long)v.f_bavail * v.f_frsize;
    unsigned long long used  = total - freeb;
    if (total == 0) { print_block("Disk", "N/A"); return; }
    double pct = (double)used / (double)total * 100.0;

    const char* unit;
    double divisor;
    if (total >= 1024ULL * 1024 * 1024 * 1024) { divisor = 1024.0*1024*1024*1024; unit = "TiB"; }
    else                                        { divisor = 1024.0*1024*1024;      unit = "GiB"; }
    double u = (double)used  / divisor;
    double t = (double)total / divisor;

    char buf[128];
    snprintf(buf, sizeof(buf), "%.2f / %.2f %s (%.0f%%)", u, t, unit, pct);
    print_block("Disk", buf);
}
