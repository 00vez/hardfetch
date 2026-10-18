#include "display.h"
#include "debug.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define EDID_SIZE 128

static bool edid_is_valid(const unsigned char* e, size_t len)
{
    static const unsigned char header[8] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
    if (len < EDID_SIZE) return false;
    return memcmp(e, header, 8) == 0;
}

/* 0xFC descriptor: monitor model name, 13 bytes, newline terminated. */
static void edid_get_name(const unsigned char* e, char* out, size_t outLen)
{
    unsigned int i;
    out[0] = '\0';
    for (i = 0x36; i + 0x12 < 0x7E; i += 0x12) {
        if (e[i] != 0x00) continue;              /* timing descriptor, skip */
        if (e[i + 3] != 0xFC) continue;          /* not the model name tag */
        {
            size_t n = 0;
            while (n < 13 && e[i + 5 + n] != 0x0A && e[i + 5 + n] != 0x00) {
                if (n + 1 < outLen) out[n] = (char)e[i + 5 + n];
                n++;
            }
            while (n > 0 && out[n - 1] == ' ') n--;
            out[n < outLen ? n : outLen - 1] = '\0';
            return;
        }
    }
}

/* Physical size in mm. Some panels report nonsense in the detailed timing
   descriptor, so cross-check it against the basic display parameters. */
static void edid_get_physical_size(const unsigned char* e, unsigned int* w, unsigned int* h)
{
    unsigned int dtdW = ((unsigned int)(e[68] & 0xF0) << 4) + e[66];
    unsigned int dtdH = ((unsigned int)(e[68] & 0x0F) << 8) + e[67];
    unsigned int bdpW = (unsigned int)e[21] * 10;
    unsigned int bdpH = (unsigned int)e[22] * 10;

    if (bdpW > 0 && bdpH > 0 &&
        abs((int)dtdW - (int)bdpW) < 10 && abs((int)dtdH - (int)bdpH) < 10) {
        *w = dtdW;
        *h = dtdH;
    } else {
        *w = bdpW;
        *h = bdpH;
    }
}

/* HKLM\SYSTEM\CurrentControlSet\Enum\<monitorDevicePath>\Device Parameters\EDID.
   The device path looks like \?\DISPLAY#DEL40xx#...#{...}; '#' becomes '\'. */
static bool read_edid(const wchar_t* monitorDevicePath, unsigned char* out, DWORD outLen)
{
    wchar_t regPath[300];
    size_t len = 300;
    const wchar_t* src = L"SYSTEM\\CurrentControlSet\\Enum";
    const wchar_t* p;
    size_t i = 0;
    HKEY hKey;
    DWORD size = outLen;
    DWORD type = 0;
    bool ok = false;

    wcscpy_s(regPath, len, src);
    p = monitorDevicePath;

    /* skip the leading \?\ */
    while (*p == L'\\' || *p == L'?') p++;
    while (*p && i + 1 < len) {
        if (*p == L'{') break;
        regPath[i++] = (*p == L'#') ? L'\\' : *p;
        p++;
    }
    wcscpy_s(regPath + i, len - i, L"Device Parameters");

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    if (RegQueryValueExW(hKey, L"EDID", NULL, &type, out, &size) == ERROR_SUCCESS &&
        type == REG_BINARY && size > 0) {
        ok = true;
        if (debug_is_enabled())
            debug_raw("EDID", out, size);
    }
    RegCloseKey(hKey);
    return ok;
}

typedef BOOL(WINAPI* PFN_GetDpiForMonitor)(HMONITOR, int, UINT*, UINT*);

static PFN_GetDpiForMonitor resolve_dpi(void)
{
    HMODULE shcapi = LoadLibraryA("shcore.dll");
    PFN_GetDpiForMonitor fn;
    if (!shcapi) return NULL;
    fn = (PFN_GetDpiForMonitor)GetProcAddress(shcapi, "GetDpiForMonitor");
    if (!fn) { FreeLibrary(shcapi); return NULL; }
    return fn;
}

void display_collect(hf_state* s)
{
    DISPLAYCONFIG_PATH_INFO paths[64];
    DISPLAYCONFIG_MODE_INFO modes[128];
    UINT32 pathCount = 64, modeCount = 128;

    debug_begin("display");

    s->display_count = 0;

    {
        LONG rc = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths,
                                     &modeCount, modes, NULL);
        debug_log("QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS) -> %ld (0x%08lX) GetLastError=%s",
                  (long)rc, (unsigned long)rc, debug_win32(GetLastError()));
        if (rc != ERROR_SUCCESS) {
            pathCount = 64;
            modeCount = 128;
            rc = QueryDisplayConfig(QDC_ALL_PATHS, &pathCount, paths,
                                    &modeCount, modes, NULL);
            debug_log("QueryDisplayConfig(QDC_ALL_PATHS) -> %ld (0x%08lX) paths=%lu modes=%lu",
                      (long)rc, (unsigned long)rc, (unsigned long)pathCount,
                      (unsigned long)modeCount);
            if (rc != ERROR_SUCCESS) return;
        }
    }
    debug_log("QueryDisplayConfig -> %lu active paths", (unsigned long)pathCount);

    for (UINT32 i = 0; i < pathCount && s->display_count < HF_MAX_DISPLAYS; i++) {
        DISPLAYCONFIG_PATH_INFO* path = &paths[i];
        DISPLAYCONFIG_SOURCE_MODE* srcMode;
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName;
        DISPLAYCONFIG_TARGET_PREFERRED_MODE prefMode;
        hf_display* d = &s->displays[s->display_count];
        unsigned char edid[EDID_SIZE];

        if (path->sourceInfo.modeInfoIdx >= modeCount) continue;
        srcMode = &modes[path->sourceInfo.modeInfoIdx].sourceMode;

        ZeroMemory(d, sizeof(*d));
        ZeroMemory(&targetName, sizeof(targetName));
        ZeroMemory(&prefMode, sizeof(prefMode));

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

        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = path->targetInfo.adapterId;
        targetName.header.id = path->targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&targetName.header) == ERROR_SUCCESS) {
            if (read_edid(targetName.monitorDevicePath, edid, sizeof(edid)) &&
                edid_is_valid(edid, sizeof(edid))) {
                edid_get_name(edid, d->name, sizeof(d->name));
                edid_get_physical_size(edid, &d->phys_w_mm, &d->phys_h_mm);
                debug_log("display '%s' edid ok, %u x %u mm", d->name,
                          d->phys_w_mm, d->phys_h_mm);
            } else {
                debug_log("display '%ls' has no usable EDID",
                          targetName.monitorFriendlyDeviceName);
                if (targetName.flags.friendlyNameFromEdid)
                    WideCharToMultiByte(CP_UTF8, 0, targetName.monitorFriendlyDeviceName,
                                        -1, d->name, (int)sizeof(d->name), NULL, NULL);
            }
        } else {
            debug_log("DisplayConfigGetDeviceInfo(TARGET_NAME) failed: %s",
                      debug_win32(GetLastError()));
        }

        prefMode.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
        prefMode.header.size = sizeof(prefMode);
        prefMode.header.adapterId = path->targetInfo.adapterId;
        prefMode.header.id = path->targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&prefMode.header) == ERROR_SUCCESS) {
            debug_log("preferred mode %ux%u @ %u/%u Hz",
                      prefMode.targetMode.targetVideoSignalInfo.activeSize.cx,
                      prefMode.targetMode.targetVideoSignalInfo.activeSize.cy,
                      prefMode.targetMode.targetVideoSignalInfo.vSyncFreq.Numerator,
                      prefMode.targetMode.targetVideoSignalInfo.vSyncFreq.Denominator);
        }

        /* Effective DPI, which is what the user actually sees under scaling. */
        {
            static PFN_GetDpiForMonitor dpiFn = NULL;
            static int dpiResolved = 0;
            if (!dpiResolved) { dpiResolved = 1; dpiFn = resolve_dpi(); }
            POINT pt = { srcMode->position.x, srcMode->position.y };
            HMONITOR hm = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
            UINT dx = 0, dy = 0;
            if (dpiFn && hm && dpiFn(hm, 0 /*MDT_EFFECTIVE_DPI*/, &dx, &dy) && dx > 0) {
                d->dpi = dx;
                debug_log("effective dpi %u x %u", dx, dy);
            } else {
                debug_log("GetDpiForMonitor unavailable for this display");
            }
        }
        if (d->dpi == 0) {
            HDC hdc = GetDC(NULL);
            int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            if (dpi > 0) d->dpi = (unsigned int)dpi;
            ReleaseDC(NULL, hdc);
        }

        debug_log("display %d '%s' %ux%u @ %u/%u Hz %ux%u mm dpi=%u primary=%d",
                  s->display_count, d->name, d->width, d->height,
                  d->refresh_num, d->refresh_den, d->phys_w_mm, d->phys_h_mm,
                  d->dpi, d->primary);

        s->display_count++;
    }

    debug_end();
}