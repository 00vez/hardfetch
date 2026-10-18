#include "../cpu_temp.h"
#include "../debug.h"
#include "../hf_state.h"

#include <dirent.h>

/* Root of the thermal sysfs tree. A macro only so a test build can point it
   at a fixture; the default is the real path. */
#ifndef HF_THERMAL_ROOT
#define HF_THERMAL_ROOT "/sys/class/thermal"
#endif

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

#if defined(__APPLE__)
static int is_arm64(void)
{
    int v = 0;
    size_t l = sizeof(v);
    if (sysctlbyname("hw.optional.arm64", &v, &l, NULL, 0) == 0) return v;
    return 0;
}
#endif

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

static int read_temperature(void)
{
    int tempC = -1;

#if defined(__APPLE__)
    if (is_arm64()) {
        if (apple_smc_cpu_temp(&tempC) != 0)
            (void)apple_hid_cpu_temp(&tempC);
    } else {
        static const char* keys[] = { "TC0D", "TC0P", NULL };
        int i;
        for (i = 0; keys[i]; i++) {
            int v = 0;
            if (apple_smc_read_temp(keys[i], &v) == 0) { tempC = v; break; }
        }
    }
#else
    /* The thermal zones are numbered and zone 0 is not always the CPU:
       pick the one whose type says so. */
    {
        DIR* d = opendir(HF_THERMAL_ROOT);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d)) != NULL) {
                char path[512];
                char type[64] = {0};
                FILE* f;
                long milli = 0;
                int isCpu;

                if (strncmp(e->d_name, "thermal_zone", 12) != 0) continue;
                snprintf(path, sizeof(path), HF_THERMAL_ROOT "/%.*s/type",
                         (int)sizeof(e->d_name), e->d_name);
                f = fopen(path, "r");
                if (f) {
                    if (fgets(type, sizeof(type), f))
                        (void)strcspn(type, "\r\n");
                    fclose(f);
                }
                isCpu = (strstr(type, "x86_pkg_temp") || strstr(type, "cpu") ||
                         strstr(type, "soc_thermal"));
                if (!isCpu) continue;

                snprintf(path, sizeof(path), HF_THERMAL_ROOT "/%.*s/temp",
                         (int)sizeof(e->d_name), e->d_name);
                f = fopen(path, "r");
                if (!f) continue;
                if (fscanf(f, "%ld", &milli) == 1) tempC = (int)(milli / 1000);
                fclose(f);
                if (tempC > 0) break;
            }
            closedir(d);
        }
    }
    if (tempC <= 0) {
        FILE* f = fopen(HF_THERMAL_ROOT "/thermal_zone0/temp", "r");
        if (f) {
            long milli = 0;
            if (fscanf(f, "%ld", &milli) == 1) tempC = (int)(milli / 1000);
            fclose(f);
        }
    }
#endif

    return tempC;
}

void cpu_temp_collect(hf_state* s)
{
    int c;

    debug_begin("temp");
    c = read_temperature();
    s->cpu_temp_c = c;
    debug_log("cpu temp = %d C", c);
    debug_end();
}
