#include "../battery.h"
#include "../debug.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif

#if !defined(__APPLE__)

/* Linux has no battery collector yet. The row stays out of the output
   instead of showing a guess, same as on a desktop without a battery. */
void battery_collect(hf_state* s)
{
    (void)s;
    debug_log("battery: no collector on this platform");
}

#else

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
    (void)read_int(bat, CFSTR("IsCharging"), &charging);
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
