#include "../output.h"
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

static int read_smc_int(const char* key) {
    int val = 0;
    io_service_t svc = IOServiceGetMatchingService(0, IOServiceNameMatching("AppleSMC"));
    if (svc) {
        CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
        if (k) {
            CFDataRef d = (CFDataRef)IORegistryEntryCreateCFProperty(svc, k, kCFAllocatorDefault, 0);
            if (d && CFGetTypeID(d) == CFDataGetTypeID()) {
                int32_t i32 = 0;
                if (CFNumberGetValue((CFNumberRef)d, kCFNumberSInt32Type, &i32)) val = (int)i32;
                CFRelease(d);
            }
            CFRelease(k);
        }
        IOObjectRelease(svc);
    }
    return val;
}

void print_battery_info(void) {
    char out[256];
    int pct = 0;
    int tempC = 0;
    int cycle = 0;
    int charging = 0;

    /* Versuch AppleSmartBattery via IOKit */
    io_service_t bat = IOServiceGetMatchingService(0, IOServiceNameMatching("AppleSmartBattery"));
    if (bat) {
        CFStringRef k1 = CFStringCreateWithCString(NULL, "BatteryChargeState", kCFStringEncodingUTF8);
        if (k1) {
            CFNumberRef n = (CFNumberRef)IORegistryEntryCreateCFProperty(bat, k1, kCFAllocatorDefault, 0);
            if (n) {
                int32_t s = 0;
                if (CFNumberGetValue(n, kCFNumberSInt32Type, &s)) charging = s;
                CFRelease(n);
            }
            CFRelease(k1);
        }
        IOObjectRelease(bat);
    }

    /* SMC Keys für Batterie (range; M-Serie ähnlich) */
    pct = read_smc_int("B0Th"); /* Versuch */
    if (pct <= 0) pct = read_smc_int("VBAT");
    tempC = read_smc_int("TB0T");
    cycle = read_smc_int("CycleCount");
    if (cycle <= 0) cycle = read_smc_int("B0Cy");

    if (pct > 0) {
        snprintf(out, sizeof(out), "%d%%", pct);
    } else {
        snprintf(out, sizeof(out), "N/A");
    }
    if (tempC >= 0) {
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), " | Temp %d°C", tempC);
        strncat(out, temp_str, sizeof(out) - strlen(out) - 1);
    }
    if (charging > 0) strncat(out, " [Charging]", sizeof(out) - strlen(out) - 1);
    if (cycle > 0) {
        char cyc[32];
        snprintf(cyc, sizeof(cyc), " | Cycles %d", cycle);
        strncat(out, cyc, sizeof(out) - strlen(out) - 1);
    }
    print_block("Battery", out[0] ? out : "N/A");
}
