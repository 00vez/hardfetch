#include "apple_smc.h"
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int apple_smc_read_int(const char* key_name, int* out_val) {
    int result = -1;
    io_service_t svc = IOServiceGetMatchingService(0, IOServiceNameMatching("AppleSMC"));
    if (svc) {
        CFStringRef key = CFStringCreateWithCString(NULL, key_name, kCFStringEncodingUTF8);
        if (key) {
            CFNumberRef val = (CFNumberRef)IORegistryEntryCreateCFProperty(svc, key, kCFAllocatorDefault, 0);
            if (val && CFGetTypeID(val) == CFNumberGetTypeID()) {
                SInt32 i32 = 0;
                if (CFNumberGetValue(val, kCFNumberSInt32Type, &i32)) {
                    *out_val = (int)i32;
                    result = 0;
                }
                CFRelease(val);
            }
            CFRelease(key);
        }
        IOObjectRelease(svc);
    }
    return result;
}
