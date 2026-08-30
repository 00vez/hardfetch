#include "../cpu_temp.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#if defined(__APPLE__)
typedef unsigned int u_int;
typedef unsigned char u_char;
typedef unsigned short u_short;
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <IOKit/hidsystem/IOHIDServiceClient.h>
#include "apple_smc.h"
#endif

static int is_arm64(void) {
#if defined(__APPLE__)
    int v = 0; size_t l = sizeof(v);
    if (sysctlbyname("hw.optional.arm64", &v, &l, NULL, 0) == 0) return v;
#endif
    return 0;
}

#if defined(__APPLE__)
static int apple_hid_cpu_temp(int *out_c) {
    void *h = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_NOW);
    if (!h) h = dlopen("/System/Library/Frameworks/IOKit.framework/Versions/A/IOKit", RTLD_NOW);
    if (!h) return -1;
    typedef void* (*CreateFunc)(CFAllocatorRef);
    typedef void (*SetMatchFunc)(void*, CFDictionaryRef);
    typedef CFArrayRef (*CopyServFunc)(void*);
    typedef void* (*CopyEventFunc)(void*, int64_t, int32_t, int64_t);
    typedef double (*GetFloatFunc)(void*, int32_t);
    typedef CFStringRef (*CopyPropFunc)(void*, CFStringRef);
    CreateFunc create = (CreateFunc)dlsym(h, "IOHIDEventSystemClientCreate");
    SetMatchFunc setMatching = (SetMatchFunc)dlsym(h, "IOHIDEventSystemClientSetMatching");
    CopyServFunc copyServices = (CopyServFunc)dlsym(h, "IOHIDEventSystemClientCopyServices");
    CopyEventFunc copyEvent = (CopyEventFunc)dlsym(h, "IOHIDServiceClientCopyEvent");
    GetFloatFunc getFloat = (GetFloatFunc)dlsym(h, "IOHIDEventGetFloatValue");
    CopyPropFunc copyProp = (CopyPropFunc)dlsym(h, "IOHIDServiceClientCopyProperty");
    if (!create || !setMatching || !copyServices || !copyEvent || !getFloat || !copyProp) return -1;
    const int page = 0xff00, usage = 5;
    CFNumberRef pg = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
    CFNumberRef us = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    const void *keys[2] = { CFSTR("PrimaryUsagePage"), CFSTR("PrimaryUsage") };
    const void *vals[2] = { pg, us };
    CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(pg); CFRelease(us);
    void *sys = create(kCFAllocatorDefault);
    if (!sys) { CFRelease(dict); return -1; }
    setMatching(sys, dict);
    CFRelease(dict);
    CFArrayRef services = copyServices(sys);
    if (!services) { CFRelease(sys); return -1; }
    long n = CFArrayGetCount(services);
    double best = -1e9;
    bool found = false;
    for (long i = 0; i < n; i++) {
        void *svc = (void*)CFArrayGetValueAtIndex(services, i);
        CFStringRef prod = copyProp(svc, CFSTR("Product"));
        char name[256] = {0};
        if (prod) {
            CFStringGetCString(prod, name, sizeof(name), kCFStringEncodingUTF8);
            CFRelease(prod);
        }
        // filter: only PMU t* sensors, skip battery/gas gauge/NAND
        if (strstr(name, "battery") || strstr(name, "gas gauge") || strstr(name, "NAND")) continue;
        if (strncmp(name, "PMU", 3) != 0) continue;
        void *ev = copyEvent(svc, 15, 0, 0);
        if (!ev) continue;
        double t = getFloat(ev, 15 << 16);
        CFRelease(ev);
        if (t < 5 || t > 120) continue;
        if (t > best) { best = t; found = true; }
    }
    CFRelease(services);
    CFRelease(sys);
    // do not dlclose(h) to keep symbols valid
    if (!found) return -1;
    *out_c = (int)(best + 0.5);
    return 0;
}
#endif

void print_cpu_temp_power(double load)
{
    char detail[128];
    int tempC = -1;
#if defined(__APPLE__)
    if (is_arm64()) {
        if (apple_smc_cpu_temp(&tempC) != 0) apple_hid_cpu_temp(&tempC);
    } else {
        const char *keys[] = {"TC0D","TC0P",NULL};
        for (int i=0; keys[i]; i++) { int v; if (apple_smc_read_temp(keys[i],&v)==0){tempC=v;break;} }
    }
#else
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int milli = 0;
        if (fscanf(f, "%d", &milli) == 1) tempC = milli / 1000;
        fclose(f);
    }
#endif

    int pos;
    if (load >= 0) pos = snprintf(detail, sizeof(detail), "Load  %.1f%%", load);
    else pos = snprintf(detail, sizeof(detail), "Load  N/A");
    if (tempC >= 0)
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  %d\xc2\xb0""C", tempC);
    else
        pos += snprintf(detail + pos, sizeof(detail) - pos, "   |  Temp  N/A");
    print_detail(detail);
}
