#include "fmt.h"

#include <stdio.h>

#define KIB 1024ULL
#define MIB (KIB * 1024ULL)
#define GIB (MIB * 1024ULL)
#define TIB (GIB * 1024ULL)
#define PIB (TIB * 1024ULL)

static const char* pick_unit(unsigned long long bytes, unsigned long long* divisor)
{
    if (bytes >= PIB) { *divisor = PIB; return "PiB"; }
    if (bytes >= TIB) { *divisor = TIB; return "TiB"; }
    if (bytes >= GIB) { *divisor = GIB; return "GiB"; }
    if (bytes >= MIB) { *divisor = MIB; return "MiB"; }
    if (bytes >= KIB) { *divisor = KIB; return "KiB"; }
    *divisor = 1;
    return "B";
}

void fmt_bytes(char* out, size_t n, unsigned long long bytes)
{
    unsigned long long divisor = 1;
    const char* unit = pick_unit(bytes, &divisor);
    if (divisor == 1)
        snprintf(out, n, "%llu %s", bytes, unit);
    else
        snprintf(out, n, "%.2f %s", (double)bytes / (double)divisor, unit);
}

void fmt_bytes_pair(char* out, size_t n, unsigned long long used, unsigned long long total)
{
    unsigned long long divisor = 1;
    const char* unit = pick_unit(total > used ? total : used, &divisor);
    if (divisor == 1)
        snprintf(out, n, "%llu / %llu %s", used, total, unit);
    else
        snprintf(out, n, "%.2f / %.2f %s",
                 (double)used / (double)divisor,
                 (double)total / (double)divisor,
                 unit);
}

void fmt_pct(char* out, size_t n, double fraction)
{
    double p = fraction * 100.0;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;
    snprintf(out, n, "%.0f%%", p);
}

void fmt_freq(char* out, size_t n, unsigned int mhz)
{
    if (mhz == 0) {
        snprintf(out, n, "N/A");
        return;
    }
    if (mhz < 1000)
        snprintf(out, n, "%u MHz", mhz);
    else
        snprintf(out, n, "%.2f GHz", mhz / 1000.0);
}

void fmt_uptime(char* out, size_t n, unsigned long long seconds)
{
    unsigned long long days = seconds / 86400ULL;
    unsigned long long hours = (seconds / 3600ULL) % 24ULL;
    unsigned long long minutes = (seconds / 60ULL) % 60ULL;

    if (days > 0)
        snprintf(out, n, "%llud %02lluh %02llum", days, hours, minutes);
    else if (hours > 0)
        snprintf(out, n, "%lluh %02llum", hours, minutes);
    else if (minutes > 0)
        snprintf(out, n, "%llum", minutes);
    else
        snprintf(out, n, "%llus", seconds);
}