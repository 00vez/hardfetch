#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>
#if defined(__APPLE__)
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

void print_cpu_temp_power(double load)
{
    char detail[128];
    int tempC = -1;
#if defined(__APPLE__)
    io_service_t smc = IOServiceGetMatchingService(0, IOServiceNameMatching("AppleSMC"));
    if (smc) {
        CFStringRef key = CFStringCreateWithCString(NULL, "TC0D", kCFStringEncodingUTF8);
        CFDataRef d = (CFDataRef)IORegistryEntryCreateCFProperty(smc, key, kCFAllocatorDefault, 0);
        if (d && CFGetTypeID(d) == CFDataGetTypeID()) {
            uint8_t* p = (uint8_t*)CFDataGetBytePtr(d);
            CFIndex n = CFDataGetLength(d);
            if (n == 2 && p[0] == 0 && p[1] == 0) { /* valid */ }
            else if (n >= 1) tempC = (int)(int8_t)p[n-1];
        }
        if (d) CFRelease(d);
        CFRelease(key);
        IOObjectRelease(smc);
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
