#include "display.h"
#include "debug.h"
#include "edid.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/* Path building avoids backslash literals on purpose: they are easy to get
   wrong when the text passes through tooling, and a missing separator turns
   into a silent "key not found". */
#define BS 0x5C

typedef struct {
    wchar_t buf[320];
    size_t  len;
} keybuf;

static void kb_init(keybuf* k)
{
    k->len = 0;
    k->buf[0] = 0;
}

static void kb_lit(keybuf* k, const wchar_t* s)
{
    while (*s && k->len + 1 < sizeof(k->buf) / sizeof(k->buf[0]))
        k->buf[k->len++] = *s++;
    k->buf[k->len] = 0;
}

static void kb_ch(keybuf* k, wchar_t c)
{
    if (k->len + 1 < sizeof(k->buf) / sizeof(k->buf[0]))
        k->buf[k->len++] = c;
    k->buf[k->len] = 0;
}

/* EDID lives at
   HKLM \ SYSTEM \ CurrentControlSet \ Enum \ <monitorDevicePath> \ Device Parameters \ EDID
   The device path from the display API looks like \?\DISPLAY#DEL40xx#...#{GUID}
   where '#' maps to a separator. Reading it straight from the registry also
   works in sessions the display configuration API refuses, which is the
   whole point of having this path. */

int registry_edid(const wchar_t* monitorDevicePath, unsigned char* out, DWORD* outLen)
{
    keybuf k;
    const wchar_t* p;
    HKEY hKey;
    DWORD size;
    DWORD type = 0;
    LONG rc;

    if (!monitorDevicePath || !out || !outLen || *outLen == 0)
        return 0;

    kb_init(&k);
    kb_lit(&k, L"SYSTEM");
    kb_ch(&k, BS);
    kb_lit(&k, L"CurrentControlSet");
    kb_ch(&k, BS);
    kb_lit(&k, L"Enum");
    kb_ch(&k, BS);

    p = monitorDevicePath;
    while (*p == BS || *p == L'?') p++;
    while (*p && *p != L'{') {
        kb_ch(&k, (*p == L'#') ? BS : *p);
        p++;
    }
    if (k.len == 0)
        return 0;

    kb_ch(&k, BS);
    kb_lit(&k, L"Device Parameters");

    rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, k.buf, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS) {
        debug_log("RegOpenKeyExW('%ls') rc=%ld", k.buf, (long)rc);
        return 0;
    }

    size = *outLen;
    if (RegQueryValueExW(hKey, L"EDID", NULL, &type, out, &size) == ERROR_SUCCESS &&
        type == REG_BINARY && size > 0) {
        if (debug_is_enabled())
            debug_raw("EDID", out, size);
        *outLen = size;
        RegCloseKey(hKey);
        return 1;
    }

    RegCloseKey(hKey);
    return 0;
}

/* Live mode. The only source that reports the mode actually in use rather
   than what the panel could do. Needs an interactive session. */
static int live_modes(hf_state* s)
{
    DISPLAYCONFIG_PATH_INFO paths[64];
    DISPLAYCONFIG_MODE_INFO modes[128];
    UINT32 pathCount = 64, modeCount = 128;
    LONG rc;
    int added = 0;

    rc = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths,
                            &modeCount, modes, NULL);
    if (rc != ERROR_SUCCESS) {
        debug_log("QueryDisplayConfig rc=%ld (0x%08lX)", (long)rc, (unsigned long)rc);
        return 0;
    }
    debug_log("QueryDisplayConfig -> %lu active paths", (unsigned long)pathCount);

    for (UINT32 i = 0; i < pathCount && added < HF_MAX_DISPLAYS; i++) {
        DISPLAYCONFIG_PATH_INFO* path = &paths[i];
        DISPLAYCONFIG_SOURCE_MODE* srcMode;
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName;
        hf_display* d = &s->displays[added];
        unsigned char edid[512];
        DWORD edidLen = (DWORD)sizeof(edid);

        if (path->sourceInfo.modeInfoIdx >= modeCount) continue;
        srcMode = &modes[path->sourceInfo.modeInfoIdx].sourceMode;

        ZeroMemory(d, sizeof(*d));
        d->width = srcMode->width;
        d->height = srcMode->height;
        if (path->targetInfo.rotation == DISPLAYCONFIG_ROTATION_ROTATE90 ||
            path->targetInfo.rotation == DISPLAYCONFIG_ROTATION_ROTATE270) {
            unsigned int t = d->width;
            d->width = d->height;
            d->height = t;
        }
        d->refresh_num = path->targetInfo.refreshRate.Numerator;
        d->refresh_den = path->targetInfo.refreshRate.Denominator;
        d->primary = (srcMode->position.x == 0 && srcMode->position.y == 0) ? 1 : 0;

        ZeroMemory(&targetName, sizeof(targetName));
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = path->targetInfo.adapterId;
        targetName.header.id = path->targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS) {
            if (registry_edid(targetName.monitorDevicePath, edid, &edidLen) &&
                edid_is_valid(edid, edidLen)) {
                edid_name(edid, d->name, sizeof(d->name));
                edid_physical_size(edid, &d->phys_w_mm, &d->phys_h_mm);
            } else if (targetName.flags.friendlyNameFromEdid) {
                WideCharToMultiByte(CP_UTF8, 0, targetName.monitorFriendlyDeviceName,
                                    -1, d->name, (int)sizeof(d->name), NULL, NULL);
            }
        }

        debug_log("display %d '%s' %ux%u @ %u/%u Hz %ux%u mm primary=%d",
                  added, d->name, d->width, d->height, d->refresh_num,
                  d->refresh_den, d->phys_w_mm, d->phys_h_mm, d->primary);
        added++;
    }
    return added;
}

/* Sessions without display configuration access still have their monitors
   in the registry. Resolution and rate stay unknown there, because the only
   source for those is the API that just refused; reporting a guessed mode
   would be worse than reporting none. */
static int registry_monitors(hf_state* s)
{
    HKEY enumKey, subKey;
    int added = 0;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY", 0, KEY_READ, &enumKey)
        != ERROR_SUCCESS) {
        debug_log("RegOpenKeyExW(Enum DISPLAY) failed");
        return 0;
    }

    for (DWORD i = 0; i < 64 && added < HF_MAX_DISPLAYS; i++) {
        wchar_t name[256];
        DWORD nameLen = (DWORD)(sizeof(name) / sizeof(name[0]));

        name[0] = 0;
        if (RegEnumKeyExW(enumKey, i, name, &nameLen, NULL, NULL, NULL, NULL)
            != ERROR_SUCCESS)
            break;
        if (!name[0]) continue;

        if (RegOpenKeyExW(enumKey, name, 0, KEY_READ, &subKey) == ERROR_SUCCESS) {
            for (DWORD j = 0; j < 16 && added < HF_MAX_DISPLAYS; j++) {
                wchar_t inst[256];
                DWORD instLen = (DWORD)(sizeof(inst) / sizeof(inst[0]));
                keybuf path;
                unsigned char edid[512];
                DWORD edidLen = (DWORD)sizeof(edid);
                hf_display* d;

                inst[0] = 0;
                if (RegEnumKeyExW(subKey, j, inst, &instLen, NULL, NULL,
                                  NULL, NULL) != ERROR_SUCCESS)
                    break;
                if (!inst[0]) continue;

                kb_init(&path);
                kb_ch(&path, BS);
                kb_lit(&path, L"DISPLAY");
                kb_ch(&path, BS);
                kb_lit(&path, name);
                kb_ch(&path, BS);
                kb_lit(&path, inst);

                if (!registry_edid(path.buf, edid, &edidLen)) continue;
                if (!edid_is_valid(edid, edidLen)) continue;

                /* One panel can be registered under several instance keys. */
                {
                    char nameBuf[64];
                    int dup = 0;
                    edid_name(edid, nameBuf, sizeof(nameBuf));
                    for (int k = 0; k < added; k++)
                        if (strcmp(s->displays[k].name, nameBuf) == 0) dup = 1;
                    if (dup) continue;
                }

                d = &s->displays[added];
                ZeroMemory(d, sizeof(*d));
                edid_name(edid, d->name, sizeof(d->name));
                edid_physical_size(edid, &d->phys_w_mm, &d->phys_h_mm);
                d->primary = (added == 0);
                debug_log("registry monitor '%s' %ux%u mm",
                          d->name, d->phys_w_mm, d->phys_h_mm);
                added++;
            }
            RegCloseKey(subKey);
        }
    }
    RegCloseKey(enumKey);
    return added;
}

void display_collect(hf_state* s)
{
    int n;

    debug_begin("display");
    s->display_count = 0;

    n = live_modes(s);
    if (n == 0) {
        debug_log("no live display configuration, falling back to registry EDID");
        n = registry_monitors(s);
    }
    s->display_count = n;

    debug_log("display_collect -> %d display(s)", n);
    debug_end();
}
