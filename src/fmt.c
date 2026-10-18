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

/* ---------- utilisation severity ---------- */

hf_load_level fmt_load_level(double fraction)
{
    if (fraction < 0.0) return HF_LOAD_NORMAL;
    if (fraction >= 0.95) return HF_LOAD_DARK_RED;
    if (fraction >= 0.85) return HF_LOAD_LIGHT_RED;
    if (fraction >= 0.70) return HF_LOAD_ORANGE;
    if (fraction >= 0.50) return HF_LOAD_LIGHT_ORANGE;
    return HF_LOAD_NORMAL;
}

/* 256-colour cube: amber -> orange -> salmon -> deep red. */
static const char COLOURS[][12] = {
    "",                  /* normal: inherit the surrounding colour */
    "\x1b[38;5;215m",   /* #FFAF00 light orange */
    "\x1b[38;5;208m",   /* #FF8700 orange      */
    "\x1b[38;5;203m",   /* #FF5F5F light red   */
    "\x1b[38;5;88m"     /* #870000 dark red    */
};

const char* fmt_load_colour(hf_load_level level)
{
    if (level < 0 || level > HF_LOAD_DARK_RED) level = HF_LOAD_NORMAL;
    return COLOURS[level];
}

void fmt_load(char* out, size_t n, double fraction, const char* suffix)
{
    double p = fraction * 100.0;
    const char* colour;

    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;
    if (suffix && suffix[0] == 0) suffix = 0;   /* no trailing space */

    colour = fmt_load_colour(fmt_load_level(fraction));
    if (colour[0] == 0) {
        if (suffix) snprintf(out, n, "%.0f%% %s", p, suffix);
        else        snprintf(out, n, "%.0f%%", p);
    } else {
        if (suffix) snprintf(out, n, "%s%.0f%% %s[0m", colour, p, suffix);
        else        snprintf(out, n, "%s%.0f%%[0m", colour, p);
    }
}

/* ---------- temperature severity ---------- */

hf_temp_level fmt_temp_level(int celsius)
{
    if (celsius >= 90) return HF_TEMP_CRITICAL;
    if (celsius >= 80) return HF_TEMP_VERY_HOT;
    if (celsius >= 70) return HF_TEMP_HOT;
    if (celsius >= 60) return HF_TEMP_WARM;
    return HF_TEMP_COOL;
}

void fmt_temp(char* out, size_t n, int celsius)
{
    /* U+00B0 DEGREE SIGN */
    static const char DEGREE[3] = { (char)0xC2u, (char)0xB0u, 0 };
    const char* colour = fmt_load_colour((hf_load_level)fmt_temp_level(celsius));

    if (colour[0] == 0)
        snprintf(out, n, "%d %sC", celsius, DEGREE);
    else
        snprintf(out, n, "%s%d %sC[0m", colour, celsius, DEGREE);
}
