#ifndef HF_FMT_H
#define HF_FMT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Single byte-size unit for a value, e.g. "11.9 GiB". */
void fmt_bytes(char* out, size_t n, unsigned long long bytes);

/* Binary byte pair sharing one unit, e.g. "1.2 / 11.9 GiB". */
void fmt_bytes_pair(char* out, size_t n, unsigned long long used, unsigned long long total);

/* Fraction 0.0..1.0 -> "32%". */
void fmt_pct(char* out, size_t n, double fraction);

/* Utilisation severity ladder, from cool to hot. Used for every load,
   utilisation and fill percentage. */
typedef enum {
    HF_LOAD_NORMAL = 0,
    HF_LOAD_LIGHT_ORANGE,
    HF_LOAD_ORANGE,
    HF_LOAD_LIGHT_RED,
    HF_LOAD_DARK_RED
} hf_load_level;

hf_load_level fmt_load_level(double fraction);
const char* fmt_load_colour(hf_load_level level);

/* "<colour><pct>% <suffix><reset>"; suffix may be 0. */
void fmt_load(char* out, size_t n, double fraction, const char* suffix);

/* MHz below 1000 stays in MHz, above becomes GHz with 2 decimals. */
void fmt_freq(char* out, size_t n, unsigned int mhz);

/* Seconds -> "45s" / "12m" / "5h 03m" / "3d 04h". */
void fmt_uptime(char* out, size_t n, unsigned long long seconds);

#ifdef __cplusplus
}
#endif

#endif