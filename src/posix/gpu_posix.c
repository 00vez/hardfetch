#define _GNU_SOURCE
#include "../gpu.h"
#include "../debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include "apple_pmgr.h"
#endif

#include <dlfcn.h>
#include <dirent.h>

/* Root of the DRM sysfs tree, a macro only so a test build can point it at
   a fixture. */
#ifndef HF_DRM_ROOT
#define HF_DRM_ROOT "/sys/class/drm"
#endif
#include <ctype.h>

/* Minimal NVML, same signatures the Windows collector uses. */
typedef enum { NVML_SUCCESS = 0 } nvmlReturn_t;
typedef struct nvmlDevice_st* nvmlDevice_t;
typedef enum { NVML_TEMPERATURE_GPU = 0 } nvmlTemperatureSensors_t;
typedef struct { unsigned long long total, free, used; } nvmlMemory_t;
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

static void* g_lib = NULL;
static PFN_init     p_init;
static PFN_shutdown p_shutdown;
static PFN_count    p_count;
static PFN_handle   p_handle;
static PFN_name     p_name;
static PFN_clock    p_clock;
static PFN_util     p_util;
static PFN_mem      p_mem;
static PFN_temp     p_temp;
static PFN_power    p_power;
static int g_nvmlReady = 0;

/* Kept loaded across calls on purpose: unloading and reloading NVML costs
   milliseconds on every --live tick for no benefit, the same reason the
   Windows collector stopped tearing it down. */
static int load_nvml(void)
{
    static const char* paths[] = {
        "/usr/lib/wsl/lib/libnvidia-ml.so.1",
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1",
        "libnvidia-ml.so.1"
    };
    size_t i;

    if (g_nvmlReady) return 1;
    if (g_lib) return 0;

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        g_lib = dlopen(paths[i], RTLD_NOW);
        if (g_lib) break;
    }
    if (!g_lib) {
        debug_log("libnvidia-ml not present");
        return 0;
    }

    p_init     = (PFN_init)dlsym(g_lib, "nvmlInit_v2");
    p_shutdown = (PFN_shutdown)dlsym(g_lib, "nvmlShutdown");
    p_count    = (PFN_count)dlsym(g_lib, "nvmlDeviceGetCount_v2");
    p_handle   = (PFN_handle)dlsym(g_lib, "nvmlDeviceGetHandleByIndex_v2");
    p_name     = (PFN_name)dlsym(g_lib, "nvmlDeviceGetName");
    p_clock    = (PFN_clock)dlsym(g_lib, "nvmlDeviceGetClockInfo");
    if (!p_clock) p_clock = (PFN_clock)dlsym(g_lib, "nvmlDeviceGetMaxClockInfo");
    p_util     = (PFN_util)dlsym(g_lib, "nvmlDeviceGetUtilizationRates");
    p_mem      = (PFN_mem)dlsym(g_lib, "nvmlDeviceGetMemoryInfo");
    p_temp     = (PFN_temp)dlsym(g_lib, "nvmlDeviceGetTemperature");
    p_power    = (PFN_power)dlsym(g_lib, "nvmlDeviceGetPowerUsage");

    if (!p_init || !p_count || !p_handle || !p_name) {
        debug_log("NVML exports missing");
        return 0;
    }
    if (p_init() != NVML_SUCCESS) {
        debug_log("nvmlInit_v2 failed");
        return 0;
    }
    g_nvmlReady = 1;
    debug_log("NVML ready");
    return 1;
}

static void collect_nvidia(hf_state* s)
{
    unsigned int count = 0;

    if (!load_nvml()) return;
    if (p_count(&count) != NVML_SUCCESS) return;
    if (count > HF_MAX_GPUS) count = HF_MAX_GPUS;

    for (unsigned int i = 0; i < count && s->gpu_count < HF_MAX_GPUS; i++) {
        nvmlDevice_t dev;
        hf_gpu* g = &s->gpus[s->gpu_count];
        nvmlMemory_t m;

        if (p_handle(i, &dev) != NVML_SUCCESS) continue;
        memset(g, 0, sizeof(*g));
        g->integrated = 0;
        g->vendor_id = 0x10DE;

        if (p_name(dev, g->name, (unsigned int)sizeof(g->name)) != NVML_SUCCESS)
            snprintf(g->name, sizeof(g->name), "%s", "NVIDIA GPU");

        if (p_clock) {
            if (p_clock(dev, NVML_CLOCK_GRAPHICS, &g->core_mhz) != 0) g->core_mhz = 0;
            if (p_clock(dev, NVML_CLOCK_MEM, &g->mem_mhz) != 0) g->mem_mhz = 0;
        }
        if (p_util) {
            nvmlUtilization_t u;
            memset(&u, 0, sizeof(u));
            if (p_util(dev, &u) == NVML_SUCCESS) g->load_pct = u.gpu;
        }
        if (p_mem) {
            memset(&m, 0, sizeof(m));
            if (p_mem(dev, &m) == NVML_SUCCESS) {
                g->vram_used = m.used;
                g->vram_total = m.total;
            }
        }
        if (p_temp) {
            unsigned int t = 0;
            if (p_temp(dev, NVML_TEMPERATURE_GPU, &t) == 0) g->temp_c = t;
        }
        if (p_power) {
            unsigned int mw = 0;
            if (p_power(dev, &mw) == 0) g->power_w = mw / 1000;
        }

        g->have_nvml = 1;
        debug_log("nvml[%u] '%s' core=%u mem=%u load=%u vram=%llu/%llu temp=%u power=%u",
                  i, g->name, g->core_mhz, g->mem_mhz, g->load_pct,
                  g->vram_used, g->vram_total, g->temp_c, g->power_w);
        s->gpu_count++;
    }
}

#if defined(__APPLE__)
extern uint32_t appleMaxFreqMHz(const char* prop);

static void collect_apple(hf_state* s)
{
    hf_gpu* g = &s->gpus[s->gpu_count];
    char brand[256] = {0};
    size_t bl = sizeof(brand);
    char* suf;

    memset(g, 0, sizeof(*g));
    g->integrated = 1;

    if (sysctlbyname("machdep.cpu.brand_string", brand, &bl, NULL, 0) == 0) {
        suf = strstr(brand, "-Core");
        if (suf) *suf = 0;
    }
    snprintf(g->name, sizeof(g->name), "%s", brand[0] ? brand : "Apple GPU");

    /* Apple Silicon exposes no VRAM figure; the unified memory size is the
       closest honest answer. */
    {
        uint64_t mem = 0;
        size_t len = sizeof(mem);
        if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) == 0 && mem > 0)
            g->vram_total = mem;
    }
    debug_log("apple gpu '%s' unified=%llu", g->name, g->vram_total);
    s->gpu_count++;
}
#endif

static const char* vendor_name(unsigned int v)
{
    switch (v) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x14E4: return "Broadcom";
        case 0x1969: return "Qualcomm";
        case 0x1B36: return "Red Hat";   /* QXL paravirtual display */
        case 0x1AF4: return "Virtio";
        case 0x1234: return "QEMU";      /* standard VGA */
        case 0x1111: return "QEMU";
        default:     return "Unknown";
    }
}

static int read_hex(const char* path, unsigned int* out)
{
    char buf[32] = {0};
    FILE* f = fopen(path, "r");
    unsigned int v = 0;

    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    if (sscanf(buf, "0x%x", &v) != 1 && sscanf(buf, "%x", &v) != 1) return -1;
    *out = v;
    return 0;
}

static int read_kb(const char* path, unsigned long long* out)
{
    char buf[64] = {0};
    FILE* f = fopen(path, "r");
    unsigned long long v = 0;

    if (!f) return -1;
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1; }
    fclose(f);
    if (sscanf(buf, "%llu", &v) != 1) return -1;
    *out = v;
    return 0;
}

/* Everything the kernel exposes without a vendor library, so an AMD or Intel
   adapter still gets a name and a memory figure. */
static void collect_drm(hf_state* s)
{
    DIR* d = opendir(HF_DRM_ROOT);

    if (!d) {
        debug_log("no " HF_DRM_ROOT);
        return;
    }

    {
        struct dirent* e;
        while ((e = readdir(d)) != NULL && s->gpu_count < HF_MAX_GPUS) {
            const char* p;
            unsigned int vid = 0, did = 0;
            unsigned long long mem = 0;
            hf_gpu* g = &s->gpus[s->gpu_count];
            int haveName = 0;

            if (strncmp(e->d_name, "card", 4) != 0) continue;
            p = e->d_name + 4;
            if (!isdigit((unsigned char)*p)) continue;

            {
                char path[512];
                snprintf(path, sizeof(path), HF_DRM_ROOT "/%.*s/device/vendor",
                         (int)sizeof(e->d_name), e->d_name);
                if (read_hex(path, &vid) != 0) continue;
                if (vid == 0x10DE) continue;   /* already covered through NVML */
            }

            memset(g, 0, sizeof(*g));
            g->vendor_id = vid;

            {
                char path[512];
                snprintf(path, sizeof(path), HF_DRM_ROOT "/%.*s/device/device",
                         (int)sizeof(e->d_name), e->d_name);
                (void)read_hex(path, &did);
                g->device_id = did;
            }
            {
                char path[512];
                snprintf(path, sizeof(path), HF_DRM_ROOT "/%.*s/device/mem_info_vram_total",
                         (int)sizeof(e->d_name), e->d_name);
                if (read_kb(path, &mem) == 0 && mem > 0)
                    g->vram_total = mem * 1024ULL;
            }
            if (g->vram_total == 0) {
                char path[512];
                snprintf(path, sizeof(path), HF_DRM_ROOT "/%.*s/device/mem_info_gtt_total",
                         (int)sizeof(e->d_name), e->d_name);
                if (read_kb(path, &mem) == 0 && mem > 0)
                    g->vram_total = mem * 1024ULL;
            }

            snprintf(g->name, sizeof(g->name), "%s Graphics (PCI %04X:%04X)",
                     vendor_name(vid), vid, did);
            haveName = 1;

            /* An Intel or AMD part behind the same PCI id as an NVML entry is
               the same physical card, not a second one. */
            if (haveName) {
                int dup = 0;
                for (int i = 0; i < s->gpu_count; i++)
                    if (s->gpus[i].vendor_id == vid && s->gpus[i].device_id == did)
                        dup = 1;
                if (dup) continue;
            }

            debug_log("drm '%s' vendor=%04X device=%04X vram=%llu",
                      e->d_name, vid, did, g->vram_total);
            s->gpu_count++;
        }
    }
    closedir(d);
}

void gpu_collect(hf_state* s)
{
    debug_begin("gpu");
    s->gpu_count = 0;

#if defined(__APPLE__)
    collect_apple(s);
#else
    collect_nvidia(s);
    collect_drm(s);
#endif

    for (int i = 0; i < s->gpu_count; i++)
        debug_log("gpu[%d] '%s' %s nvml=%d", i, s->gpus[i].name,
                  s->gpus[i].integrated ? "integrated" : "discrete",
                  s->gpus[i].have_nvml);

    debug_log("gpu_collect -> %d adapter(s)", s->gpu_count);
    debug_end();
}
