#define _GNU_SOURCE
#include "../display.h"
#include "../debug.h"
#include "../edid.h"

#include <dirent.h>
#include <stdio.h>

/* Root of the DRM sysfs tree. A macro only so a test build can point it at a
   fixture; the default is the real path. */
#ifndef HF_DRM_ROOT
#define HF_DRM_ROOT "/sys/class/drm"
#endif
#include <stdlib.h>
#include <string.h>

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

void display_collect(hf_state* s)
{
    debug_begin("display");
    s->display_count = 0;
    collect_sysfs(s);
    debug_log("display_collect -> %d display(s)", s->display_count);
    debug_end();
}
