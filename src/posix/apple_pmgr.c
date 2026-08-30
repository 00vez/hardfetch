#include "apple_pmgr.h"
#if defined(__APPLE__)
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

uint32_t appleMaxFreqMHz(const char* prop) {
#if !defined(__APPLE__)
  (void)prop; return 0;
#else
    io_service_t svc = IOServiceGetMatchingService(0, IOServiceNameMatching("pmgr"));
    uint32_t max = 0;
    if (svc) {
        CFStringRef key = CFStringCreateWithCString(NULL, prop, kCFStringEncodingUTF8);
        CFDataRef d = (CFDataRef)IORegistryEntryCreateCFProperty(svc, key, kCFAllocatorDefault, 0);
        if (d && CFGetTypeID(d) == CFDataGetTypeID()) {
            uint32_t* p = (uint32_t*)CFDataGetBytePtr(d);
            CFIndex n = CFDataGetLength(d) / sizeof(uint32_t);
            for (CFIndex i = 0; i + 1 < n; i += 2)
                if (p[i] > max) max = p[i];
            max = (max > 100000000) ? max / 1000000 : max / 1000;
        }
        if (d) CFRelease(d);
        CFRelease(key);
        IOObjectRelease(svc);
    }
    return max;
#endif
    return 0;
}
