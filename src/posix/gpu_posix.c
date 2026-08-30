#define _GNU_SOURCE
#include "../gpu.h"
#include "../output.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif

#include <dlfcn.h>
#include <dirent.h>
#include <ctype.h>

/* Minimal NVML (mirrors the signatures used by the Windows gpu.cpp) */
typedef enum { NVML_SUCCESS = 0 } nvmlReturn_t;
struct nvmlDevice;
typedef struct nvmlDevice* nvmlDevice_t;
typedef enum { NVML_TEMPERATURE_GPU = 0 } nvmlTemperatureSensors_t;
typedef struct { unsigned long long total; unsigned long long free; unsigned long long used; } nvmlMemory_t;
typedef enum { NVML_CLOCK_GRAPHICS = 0, NVML_CLOCK_MEM = 2 } nvmlClockType_t;
typedef struct { unsigned int gpu; unsigned int memory; } nvmlUtilization_t;

typedef nvmlReturn_t (*PFN_init)(void);
typedef nvmlReturn_t (*PFN_shutdown)(void);
typedef nvmlReturn_t (*PFN_count)(unsigned int*);
typedef nvmlReturn_t (*PFN_handle)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (*PFN_name)(nvmlDevice_t, char*, unsigned int);
typedef nvmlReturn_t (*PFN_clock)(nvmlDevice_t, nvmlClockType_t, unsigned int*);
typedef nvmlReturn_t (*PFN_util)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t (*PFN_mem)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (*PFN_temp)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
typedef nvmlReturn_t (*PFN_power)(nvmlDevice_t, unsigned int*);

static void* g_dll = NULL;
static PFN_init     pfnInit;
static PFN_shutdown pfnShutdown;
static PFN_count    pfnCount;
static PFN_handle   pfnHandle;
static PFN_name     pfnName;
static PFN_clock    pfnClock;
static PFN_util     pfnUtil;
static PFN_mem      pfnMem;
static PFN_temp     pfnTemp;
static PFN_power    pfnPower;

static int g_idx = 0;

static int load_nvml(void)
{
    if (g_dll) return 1;
    const char* paths[] = {
        "/usr/lib/wsl/lib/libnvidia-ml.so.1",
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1",
        "libnvidia-ml.so.1"
    };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        g_dll = dlopen(paths[i], RTLD_NOW);
        if (g_dll) break;
    }
    if (!g_dll) return 0;

    pfnInit     = (PFN_init)dlsym(g_dll, "nvmlInit_v2");
    pfnShutdown = (PFN_shutdown)dlsym(g_dll, "nvmlShutdown");
    pfnCount    = (PFN_count)dlsym(g_dll, "nvmlDeviceGetCount_v2");
    pfnHandle   = (PFN_handle)dlsym(g_dll, "nvmlDeviceGetHandleByIndex_v2");
    pfnName     = (PFN_name)dlsym(g_dll, "nvmlDeviceGetName");
    pfnClock    = (PFN_clock)dlsym(g_dll, "nvmlDeviceGetMaxClockInfo");
    pfnUtil     = (PFN_util)dlsym(g_dll, "nvmlDeviceGetUtilizationRates");
    pfnMem      = (PFN_mem)dlsym(g_dll, "nvmlDeviceGetMemoryInfo");
    pfnTemp     = (PFN_temp)dlsym(g_dll, "nvmlDeviceGetTemperature");
    pfnPower    = (PFN_power)dlsym(g_dll, "nvmlDeviceGetPowerUsage");

    if (!pfnInit || !pfnShutdown || !pfnCount || !pfnHandle || !pfnName) {
        dlclose(g_dll);
        g_dll = NULL;
        return 0;
    }
    return 1;
}

#if defined(__APPLE__)
static void print_apple_gpu(void)
{
    char model[256] = {0};
    size_t len = sizeof(model);
    if (sysctlbyname("hw.model", model, &len, NULL, 0) == 0) {
        printf("Apple %s (Integrated)\n", model);
    } else {
        printf("Apple GPU (Unknown)\n");
    }
}
#endif

static void print_nvidia_gpus(void)
{
    if (!load_nvml()) return;
    if (pfnInit() != NVML_SUCCESS) return;

    unsigned int count = 0;
    if (pfnCount(&count) != NVML_SUCCESS) count = 0;
    if (count > 8) count = 8;

    for (unsigned int i = 0; i < count; i++) {
        nvmlDevice_t dev;
        if (pfnHandle(i, &dev) != NVML_SUCCESS) continue;

        char name[256] = "NVIDIA GPU";
        if (pfnName(dev, name, sizeof(name)) != NVML_SUCCESS)
            strcpy(name, "NVIDIA GPU");
        name[sizeof(name) - 1] = '\0';

        unsigned int coreClock = 0, memClock = 0, util = 0, temp = 0, powerMw = 0;
        double vramUsed = 0, vramTotal = 0;
        if (pfnClock) pfnClock(dev, NVML_CLOCK_GRAPHICS, &coreClock);
        if (pfnClock) pfnClock(dev, NVML_CLOCK_MEM, &memClock);
        if (pfnUtil) { nvmlUtilization_t u; if (pfnUtil(dev, &u) == NVML_SUCCESS) util = u.gpu; }
        if (pfnMem) {
            nvmlMemory_t m;
            if (pfnMem(dev, &m) == NVML_SUCCESS) {
                vramUsed  = m.used  / (1024.0 * 1024.0 * 1024.0);
                vramTotal = m.total / (1024.0 * 1024.0 * 1024.0);
            }
        }
        if (pfnTemp) pfnTemp(dev, NVML_TEMPERATURE_GPU, &temp);
        if (pfnPower) pfnPower(dev, &powerMw);

        char line[512];
        int pos = 0;
        pos += snprintf(line + pos, sizeof(line) - pos, "%s", name);
        if (coreClock > 0)
            pos += snprintf(line + pos, sizeof(line) - pos, " @ %.2f GHz", coreClock / 1000.0);
        if (vramTotal >= 1.0)
            pos += snprintf(line + pos, sizeof(line) - pos, " (%.2f GiB)", vramTotal);
        else if (vramTotal > 0)
            pos += snprintf(line + pos, sizeof(line) - pos, " (%.2f MiB)", vramTotal * 1024.0);
        pos += snprintf(line + pos, sizeof(line) - pos, " [Discrete]");

        char label[12];
        snprintf(label, sizeof(label), "GPU %d", ++g_idx);
        print_block_green(label, line);

        char detail[512];
        snprintf(detail, sizeof(detail),
            "Core %u MHz  |  Mem %u MHz  |  Load %u%%  |  VRAM %.1f / %.1f GiB  |  Temp %u\xc2\xb0""C  |  Power %u W",
            coreClock, memClock, util, vramUsed, vramTotal, temp, powerMw / 1000);
        print_detail(detail);
    }

    pfnShutdown();
    dlclose(g_dll);
    g_dll = NULL;
}

static const char* vendor_name(unsigned int v)
{
    switch (v) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x14E4: return "Broadcom";
        case 0x1969: return "Qualcomm";
        default:     return "Unknown";
    }
}

static int read_hex(const char* path, unsigned int* out)
{
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    char buf[16] = {0};
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    unsigned int v = 0;
    if (sscanf(buf, "0x%x", &v) != 1 && sscanf(buf, "%x", &v) != 1) return -1;
    *out = v;
    return 0;
}

static void print_drm_gpus(void)
{
    DIR* d = opendir("/sys/class/drm");
    if (!d) return;

    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "card", 4) != 0) continue;
        const char* p = e->d_name + 4;
        if (!isdigit((unsigned char)*p)) continue;

        char vpath[256];
        snprintf(vpath, sizeof(vpath), "/sys/class/drm/%s/device/vendor", e->d_name);
        unsigned int vid = 0, did = 0;
        if (read_hex(vpath, &vid) != 0) continue;
        if (vid == 0x10DE) continue; /* NVIDIA: already covered by NVML */

        char dpath[256];
        snprintf(dpath, sizeof(dpath), "/sys/class/drm/%s/device/device", e->d_name);
        (void)read_hex(dpath, &did);

        const char* vn = vendor_name(vid);
        char label[12], val[128];
        snprintf(label, sizeof(label), "GPU %d", ++g_idx);
        snprintf(val, sizeof(val), "%s (PCI %04X:%04X)", vn, vid, did);
        print_block_green(label, val);
    }
    closedir(d);
}

void print_gpu_info(void)
{
#if defined(__APPLE__)
    print_apple_gpu();
    return;
#endif
    print_nvidia_gpus();
    print_drm_gpus();
    if (g_idx == 0) print_block("GPU", "N/A");
}