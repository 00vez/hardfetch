#include "../battery.h"
#include "../debug.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif

#if !defined(__APPLE__)

/* The kernel exposes the batteries under /sys/class/power_supply, one
   directory each. A directory is a battery when its type file reads
   "Battery". The charge comes from capacity, the state from status, and
   the mains adapter from the online file of the other directories.
   A desktop has no battery directory and then the row stays out. */

#ifndef HF_POWER_SUPPLY_ROOT
#define HF_POWER_SUPPLY_ROOT "/sys/class/power_supply"
#endif

static int read_int(const char* path, long* out)
{
    FILE* f = fopen(path, "r");
    long v;

    if (!f) return 0;
    if (fscanf(f, "%ld", &v) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    *out = v;
    return 1;
}

static int read_text(const char* path, char* out, size_t n)
{
    FILE* f = fopen(path, "r");
    size_t l;

    out[0] = 0;
    if (!f) return 0;
    if (!fgets(out, (int)n, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    l = strlen(out);
    while (l > 0 && (out[l - 1] == '\n' || out[l - 1] == '\r' ||
                     out[l - 1] == ' '  || out[l - 1] == '\t'))
        out[--l] = 0;
    return 1;
}

void battery_collect(hf_state* s)
{
    DIR* dir = opendir(HF_POWER_SUPPLY_ROOT);
    struct dirent* e;
    char batteryPath[320];
    long capacity = 0;
    int haveBattery = 0;
    int onMains = 0;

    debug_begin("battery");

    if (!dir) {
        debug_log("no " HF_POWER_SUPPLY_ROOT);
        debug_end();
        return;
    }

    while ((e = readdir(dir)) != NULL) {
        char type[64];
        char path[384];

        if (e->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), HF_POWER_SUPPLY_ROOT "/%s/type", e->d_name);
        if (!read_text(path, type, sizeof(type))) continue;

        if (strcmp(type, "Battery") == 0) {
            char capPath[416];

            snprintf(batteryPath, sizeof(batteryPath),
                     HF_POWER_SUPPLY_ROOT "/%s", e->d_name);
            snprintf(capPath, sizeof(capPath), "%s/capacity", batteryPath);
            if (!read_int(capPath, &capacity)) capacity = 0;
            haveBattery = 1;
        }
        else if (strcmp(type, "Mains") == 0 || strcmp(type, "USB") == 0) {
            char onlinePath[416];
            long online = 0;
            snprintf(onlinePath, sizeof(onlinePath),
                     HF_POWER_SUPPLY_ROOT "/%s/online", e->d_name);
            if (read_int(onlinePath, &online) && online) onMains = 1;
        }
    }
    closedir(dir);

    if (!haveBattery) {
        debug_log("no battery");
        debug_end();
        return;
    }
    if (capacity < 0 || capacity > 100) {
        debug_log("capacity %ld out of range", capacity);
        debug_end();
        return;
    }

    s->have_battery = 1;
    s->battery_pct = (unsigned int)capacity;

    /* The state file says whether the battery is charging right now. When
       the machine runs on mains the state is Charging or Full. */
    {
        char status[64];
        char statusPath[416];
        snprintf(statusPath, sizeof(statusPath), "%s/status", batteryPath);
        if (read_text(statusPath, status, sizeof(status))) {
            if (strcmp(status, "Charging") == 0 ||
                strcmp(status, "Full") == 0)
                s->battery_charging = 1;
            debug_log("status '%s'", status);
        }
    }
    if (!s->battery_charging) s->battery_charging = onMains ? 1 : 0;

    debug_log("battery %u%% charging=%d", s->battery_pct, s->battery_charging);
    debug_end();
}

#else /* __APPLE__ */

/* The battery is a single IOKit service, AppleSmartBattery. Everything the
   row shows comes from there: the charge in percent from the capacity
   pair, the charging state as a boolean, the temperature in hundredths of a
   degree and the cycle count. A Mac without a battery has no such service,
   and then the row is left out. */

static int read_int(io_object_t svc, CFStringRef key, int32_t* out)
{
    CFTypeRef prop;
    int32_t v = 0;
    int ok = 0;

    prop = IORegistryEntryCreateCFProperty(svc, key, kCFAllocatorDefault, 0);
    if (prop) {
        if (CFGetTypeID(prop) == CFNumberGetTypeID()) {
            ok = CFNumberGetValue((CFNumberRef)prop, kCFNumberSInt32Type, &v);
        } else if (CFGetTypeID(prop) == CFBooleanGetTypeID()) {
            if (CFBooleanGetValue((CFBooleanRef)prop)) {
                v = 1;
                ok = 1;
            }
        }
        CFRelease(prop);
    }
    if (ok) *out = v;
    return ok;
}

void battery_collect(hf_state* s)
{
    CFMutableDictionaryRef matching;
    io_object_t bat;
    int32_t current = 0, maximum = 0, temperature = 0, cycles = 0, charging = 0;

    debug_begin("battery");

    matching = IOServiceMatching("AppleSmartBattery");
    if (!matching) {
        debug_log("IOServiceMatching failed");
        debug_end();
        return;
    }
    /* IOServiceGetMatchingService consumes the reference of the matching
       dictionary, so it must not be released here. Releasing it would
       free the object a second time. */
    bat = IOServiceGetMatchingService(kIOMainPortDefault, matching);
    if (!bat) {
        debug_log("no AppleSmartBattery");
        debug_end();
        return;
    }

    (void)read_int(bat, CFSTR("CurrentCapacity"), &current);
    (void)read_int(bat, CFSTR("MaxCapacity"), &maximum);
    if (!read_int(bat, CFSTR("IsCharging"), &charging)) {
        /* Older machines do not have the boolean, they have a state where
           two means charging on mains. */
        int32_t state = 0;
        if (read_int(bat, CFSTR("BatteryChargeState"), &state))
            charging = (state == 2);
    }
    (void)read_int(bat, CFSTR("Temperature"), &temperature);
    (void)read_int(bat, CFSTR("CycleCount"), &cycles);
    IOObjectRelease(bat);

    if (maximum <= 0 || current < 0) {
        debug_log("no capacity reported");
        debug_end();
        return;
    }

    s->have_battery = 1;
    s->battery_pct = (unsigned int)((current * 100 + maximum / 2) / maximum);
    s->battery_charging = charging ? 1 : 0;
    /* The sensor reports hundredths of a degree, and 0 means no reading. */
    s->battery_temp_c = (temperature > 0) ? (int)(temperature / 100) : -1;
    s->battery_cycles = (cycles > 0) ? (unsigned int)cycles : 0;

    debug_log("battery %u%% charging=%d temp=%d cycles=%u",
              s->battery_pct, s->battery_charging,
              s->battery_temp_c, s->battery_cycles);
    debug_end();
}

#endif
