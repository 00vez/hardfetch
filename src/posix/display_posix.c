#define _GNU_SOURCE
#include "../display.h"
#include "../debug.h"
#include "../edid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#else
#include <dirent.h>
#endif

/* Root of the DRM sysfs tree. A macro only so a test build can point it at a
   fixture; the default is the real path. */
#ifndef HF_DRM_ROOT
#define HF_DRM_ROOT "/sys/class/drm"
#endif

#if defined(__APPLE__)

/* macOS has no DRM sysfs. CoreGraphics knows the active displays, their
   mode and their refresh rate. The physical size comes from the same call,
   but only the built-in panel reports it. The monitor name needs the EDID,
   which the IOKit display service carries. With more than one display the
   services cannot be matched to the screens without guessing, so the name
   is left out then and the row shows what is actually known. */

static int apple_edid(unsigned char* out, size_t outLen, size_t* got)
{
    CFMutableDictionaryRef matching;
    io_iterator_t iter = 0;
    io_object_t obj;
    int services = 0;
    int found = 0;

    *got = 0;
    matching = IOServiceMatching("IODisplayConnect");
    if (!matching) return -1;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS)
        return -1;

    while ((obj = IOIteratorNext(iter))) {
        CFTypeRef prop;

        services++;
        prop = IORegistryEntryCreateCFProperty(obj, CFSTR("IODisplayEDID"),
                                                kCFAllocatorDefault, 0);
        if (prop) {
            if (CFGetTypeID(prop) == CFDataGetTypeID()) {
                CFIndex len = CFDataGetLength((CFDataRef)prop);
                if (!found && len > 0 && (size_t)len <= outLen) {
                    memcpy(out, CFDataGetBytePtr((CFDataRef)prop), (size_t)len);
                    *got = (size_t)len;
                    found = 1;
                }
            }
            CFRelease(prop);
        }
        IOObjectRelease(obj);
    }
    IOObjectRelease(iter);

    /* Exactly one service means exactly one screen, so the EDID belongs to
       it. More than one and the assignment would be a guess. */
    if (services != 1 || !found) return -1;
    return 0;
}

static void collect_apple(hf_state* s)
{
    CGDirectDisplayID ids[HF_MAX_DISPLAYS];
    uint32_t count = 0;
    unsigned char edid[512];
    size_t edidLen = 0;

    if (CGGetActiveDisplayList(0, NULL, &count) != kCGErrorSuccess || count == 0) {
        debug_log("no active display");
        return;
    }
    if (count > HF_MAX_DISPLAYS) count = HF_MAX_DISPLAYS;
    if (CGGetActiveDisplayList(count, ids, &count) != kCGErrorSuccess) {
        debug_log("CGGetActiveDisplayList failed");
        return;
    }

    for (uint32_t i = 0; i < count && s->display_count < HF_MAX_DISPLAYS; i++) {
        hf_display* d = &s->displays[s->display_count];
        CGDisplayModeRef mode;

        memset(d, 0, sizeof(*d));

        mode = CGDisplayCopyDisplayMode(ids[i]);
        if (mode) {
            /* The size of the mode in points, which is the resolution the
               desktop runs at. The pixel size of a Retina panel would be
               twice that and would not match what the other platforms
               report for a scaled display. */
            d->width = (unsigned int)CGDisplayModeGetWidth(mode);
            d->height = (unsigned int)CGDisplayModeGetHeight(mode);
            if (d->width == 0 || d->height == 0) {
                d->width = (unsigned int)CGDisplayModeGetPixelWidth(mode);
                d->height = (unsigned int)CGDisplayModeGetPixelHeight(mode);
            }
            {
                double hz = CGDisplayModeGetRefreshRate(mode);
                if (hz > 0.0) {
                    d->refresh_num = (unsigned int)(hz + 0.5);
                    d->refresh_den = 1;
                }
            }
            CGDisplayModeRelease(mode);
        }
        if (d->width == 0 || d->height == 0) {
            CGRect b = CGDisplayBounds(ids[i]);
            d->width = (unsigned int)b.size.width;
            d->height = (unsigned int)b.size.height;
        }
        /* CoreGraphics reports the physical size of the panel in
           millimetres. Only the built-in panel and some external ones
           report it, the rest come back at zero. */
        {
            CGSize mm = CGDisplayScreenSize(ids[i]);
            if (mm.width > 0.0 && mm.height > 0.0) {
                d->phys_w_mm = (unsigned int)(mm.width + 0.5);
                d->phys_h_mm = (unsigned int)(mm.height + 0.5);
            }
        }
        d->primary = (s->display_count == 0);

        debug_log("display %d %ux%u @ %u/%u Hz %ux%u mm",
                  s->display_count, d->width, d->height,
                  d->refresh_num, d->refresh_den,
                  d->phys_w_mm, d->phys_h_mm);
        s->display_count++;
    }

    if (s->display_count == 1 &&
        apple_edid(edid, sizeof(edid), &edidLen) == 0 &&
        edid_is_valid(edid, edidLen)) {
        hf_display* d = &s->displays[0];
        char nameBuf[64];
        unsigned int wMm = 0, hMm = 0;

        edid_name(edid, nameBuf, sizeof(nameBuf));
        if (nameBuf[0])
            snprintf(d->name, sizeof(d->name), "%.*s",
                     (int)sizeof(d->name) - 1, nameBuf);
        edid_physical_size(edid, &wMm, &hMm);
        if (wMm > 0 && hMm > 0) {
            d->phys_w_mm = wMm;
            d->phys_h_mm = hMm;
        }
        debug_log("edid name='%s' size=%ux%u mm",
                  d->name, d->phys_w_mm, d->phys_h_mm);
    }
}

#else

/* Linux has no QueryDisplayConfig. The kernel exposes the same information
   through sysfs: one directory per connector under /sys/class/drm, with the
   active mode in "modes" and the panel's EDID blob in "edid". The same EDID
   parser the Windows build uses reads name and physical size. */

static int read_bytes(const char* path, unsigned char* out, size_t outLen, size_t* got)
{
    FILE* f = fopen(path, "rb");
    size_t n;

    if (!f) return -1;
    n = fread(out, 1, outLen, f);
    fclose(f);
    if (got) *got = n;
    return (n > 0) ? 0 : -1;
}

/* "1920x1080" or "1920x1080i" is the first line, the active mode. */
static int parse_mode(const char* line, unsigned int* w, unsigned int* h,
                      unsigned int* refresh)
{
    unsigned int a = 0, b = 0, r = 0;
    const char* x = strchr(line, 'x');

    if (!x) return -1;
    if (sscanf(line, "%ux%u", &a, &b) != 2) return -1;
    {
        const char* q = strchr(x + 1, 'x');
        if (q && sscanf(q + 1, "%u", &r) == 1) { /* interlace suffix */ }
        else {
            const char* sp = strchr(x + 1, ' ');
            if (sp && sscanf(sp + 1, "%u", &r) == 1) { /* 1920x1080 60 */ }
        }
    }
    if (a == 0 || b == 0) return -1;
    *w = a;
    *h = b;
    if (refresh) *refresh = r;
    return 0;
}

static int connector_wanted(const char* name)
{
    const char* base;

    if (strncmp(name, "card", 4) != 0) return 0;
    base = name + 4;
    if (!(*base >= '0' && *base <= '9')) return 0;
    /* card0 is the card itself, card0-DP-1 and friends are connectors. */
    return strchr(base, '-') != NULL;
}

static void collect_sysfs(hf_state* s)
{
    DIR* dir = opendir(HF_DRM_ROOT);
    struct dirent* e;

    if (!dir) {
        debug_log("no " HF_DRM_ROOT);
        return;
    }

    while ((e = readdir(dir)) != NULL && s->display_count < HF_MAX_DISPLAYS) {
        char modePath[320], edidPath[320];
        char modeLine[256];
        unsigned int w = 0, h = 0, refresh = 0;
        unsigned char edid[512];
        size_t got = 0;
        hf_display* d;
        FILE* f;

        if (!connector_wanted(e->d_name)) continue;

        snprintf(modePath, sizeof(modePath), HF_DRM_ROOT "/%s/modes", e->d_name);
        f = fopen(modePath, "r");
        if (!f) continue;
        modeLine[0] = 0;
        if (fgets(modeLine, sizeof(modeLine), f))
            (void)parse_mode(modeLine, &w, &h, &refresh);
        fclose(f);
        if (w == 0 || h == 0) continue;

        d = &s->displays[s->display_count];
        memset(d, 0, sizeof(*d));
        d->width = w;
        d->height = h;
        d->refresh_num = refresh;
        d->refresh_den = refresh ? 1 : 0;
        d->primary = (s->display_count == 0);
        snprintf(d->name, sizeof(d->name), "%.*s",
                 (int)sizeof(d->name) - 1, e->d_name);

        snprintf(edidPath, sizeof(edidPath), HF_DRM_ROOT "/%s/edid", e->d_name);
        if (read_bytes(edidPath, edid, sizeof(edid), &got) == 0 &&
            edid_is_valid(edid, got)) {
            char nameBuf[64];
            edid_name(edid, nameBuf, sizeof(nameBuf));
            if (nameBuf[0])
                snprintf(d->name, sizeof(d->name), "%.*s",
                         (int)sizeof(d->name) - 1, nameBuf);
            edid_physical_size(edid, &d->phys_w_mm, &d->phys_h_mm);
        }

        debug_log("display %d '%s' %ux%u @ %u Hz %ux%u mm",
                  s->display_count, d->name, d->width, d->height,
                  refresh, d->phys_w_mm, d->phys_h_mm);
        s->display_count++;
    }
    closedir(dir);
}

#endif

void display_collect(hf_state* s)
{
    debug_begin("display");
    s->display_count = 0;
#if defined(__APPLE__)
    collect_apple(s);
#else
    collect_sysfs(s);
#endif
    debug_log("display_collect -> %d display(s)", s->display_count);
    debug_end();
}
