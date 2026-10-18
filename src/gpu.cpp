#include "gpu.h"
#include "debug.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <windows.h>

/* ------------------------------------------------------------------ *
 * D3DKMT: the SDK's shared/d3dkmthk.h does not compile in user-mode C,
 * so the small set of structures actually used is declared here and
 * checked with static_assert below.
 * ------------------------------------------------------------------ */

#pragma warning(push)
#pragma warning(disable : 4201)   /* anonymous unions are fixed by the D3DKMT ABI */

typedef struct { DWORD LowPart; LONG HighPart; } HF_LUID;
typedef union { struct { UINT Instance : 6; UINT Index : 24; UINT Unique : 2; }; UINT Value; } HF_KMT_HANDLE;
typedef struct { HF_KMT_HANDLE hAdapter; HF_LUID AdapterLuid; ULONG NumOfSources; BOOL bPrecisePresentRegionsPreferred; } HF_ADAPTERINFO;
typedef struct { ULONG NumAdapters; HF_ADAPTERINFO* pAdapters; } HF_ENUMADAPTERS2;
typedef struct { HF_KMT_HANDLE hAdapter; int Type; void* pPrivateDriverData; UINT PrivateDriverDataSize; } HF_QUERYADAPTERINFO;
typedef struct { WCHAR AdapterString[260]; WCHAR BiosString[260]; WCHAR DacType[260]; WCHAR ChipType[260]; } HF_ADAPTERREGISTRYINFO;
typedef struct { UINT BusNumber, DeviceNumber, FunctionNumber; } HF_ADAPTERADDRESS;
typedef struct { UINT VendorID, DeviceID, SubVendorID, SubSystemID, RevisionID, BusType; } HF_DEVICE_IDS;
typedef struct { UINT PhysicalAdapterIndex; HF_DEVICE_IDS DeviceIds; } HF_QUERY_DEVICE_IDS;
typedef struct { ULONGLONG DedicatedVideoMemorySize, DedicatedSystemMemorySize, SharedSystemMemorySize; } HF_SEGMENTSIZEINFO;
/* Anonymous union inside a struct: a documented MSVC extension, and the
   layout is fixed by the D3DKMT ABI, so the warning is suppressed here. */
#pragma warning(push)
#pragma warning(disable : 4201)
typedef struct {
    union {
        struct {
            UINT RenderSupported : 1;
            UINT DisplaySupported : 1;
            UINT SoftwareDevice : 1;
            UINT PostDevice : 1;
            UINT HybridDiscrete : 1;
            UINT HybridIntegrated : 1;
            UINT IndirectDisplayDevice : 1;
            UINT Paravirtualized : 1;
            UINT ACGSupported : 1;
            UINT SupportSetTimingsFromVidPn : 1;
            UINT Detachable : 1;
            UINT ComputeOnly : 1;
            UINT Reserved2 : 20;
        } Bits;
        unsigned u;
    } A;
} HF_ADAPTERTYPE;
#pragma warning(pop)

#define HF_MAX_ENUM_ADAPTERS 16
enum {
    HF_KMTQAITYPE_GETSEGMENTSIZE = 3,
    HF_KMTQAITYPE_ADAPTERADDRESS = 6,
    HF_KMTQAITYPE_ADAPTERREGISTRYINFO = 8,
    HF_KMTQAITYPE_ADAPTERTYPE = 15,
    HF_KMTQAITYPE_PHYSICALADAPTERDEVICEIDS = 31
};

static_assert(sizeof(HF_ADAPTERINFO) == 20, "D3DKMT_ADAPTERINFO layout");
static_assert(sizeof(HF_ENUMADAPTERS2) == 16, "D3DKMT_ENUMADAPTERS2 layout");
static_assert(sizeof(HF_ADAPTERREGISTRYINFO) == 2080, "D3DKMT_ADAPTERREGISTRYINFO layout");
static_assert(sizeof(HF_QUERY_DEVICE_IDS) == 28, "D3DKMT_QUERY_DEVICE_IDS layout");
static_assert(sizeof(HF_SEGMENTSIZEINFO) == 24, "D3DKMT_SEGMENTSIZEINFO layout");
static_assert(sizeof(HF_ADAPTERTYPE) == 4, "D3DKMT_ADAPTERTYPE layout");

typedef LONG (WINAPI* PFN_D3DKMTEnumAdapters2)(HF_ENUMADAPTERS2*);
typedef LONG (WINAPI* PFN_D3DKMTQueryAdapterInfo)(HF_QUERYADAPTERINFO*);

namespace {

bool query(HF_ADAPTERINFO info, int type, void* out, ULONG size)
{
    static PFN_D3DKMTQueryAdapterInfo fn = NULL;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        HMODULE gdi = GetModuleHandleA("gdi32.dll");
        if (gdi)
            fn = (PFN_D3DKMTQueryAdapterInfo)GetProcAddress(gdi, "D3DKMTQueryAdapterInfo");
    }
    if (!fn) return false;
    HF_QUERYADAPTERINFO q;
    q.hAdapter = info.hAdapter;
    q.Type = type;
    q.pPrivateDriverData = out;
    q.PrivateDriverDataSize = size;
    return fn(&q) >= 0;
}

/* ------------------------------- NVML ------------------------------- */

typedef int nvmlReturn_t;
typedef struct nvmlDevice_st* nvmlDevice_t;
typedef enum { NVML_TEMPERATURE_GPU = 0 } nvmlTemperatureSensors_t;
typedef enum { NVML_CLOCK_GRAPHICS = 0, NVML_CLOCK_MEM = 2 } nvmlClockType_t;
typedef struct { unsigned int gpu; unsigned int memory; } nvmlUtilization_t;
typedef struct { unsigned long long total; unsigned long long free; unsigned long long used; } nvmlMemory_t;
/* nvmlPciInfo_t as of NVML 2.x: 16-byte legacy bus id string, five
   unsigned ints, then a 32-byte bus id string. */
typedef struct {
    char          busIdLegacy[16];
    unsigned int  domain;
    unsigned int  bus;
    unsigned int  device;
    unsigned int  pciDeviceId;      /* (deviceId << 16) | vendorId */
    unsigned int  pciSubSystemId;
    char          busId[32];
} nvmlPciInfoShim;

typedef nvmlReturn_t (WINAPI* PFN_nvmlInit)(void);
typedef nvmlReturn_t (WINAPI* PFN_nvmlShutdown)(void);
typedef nvmlReturn_t (WINAPI* PFN_nvmlCount)(unsigned int*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlHandleByIndex)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlHandleByPciBusId)(const char*, nvmlDevice_t*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlName)(nvmlDevice_t, char*, unsigned int);
typedef nvmlReturn_t (WINAPI* PFN_nvmlPciInfo)(nvmlDevice_t, nvmlPciInfoShim*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlClock)(nvmlDevice_t, nvmlClockType_t, unsigned int*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlUtil)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlMem)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlTemp)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);
typedef nvmlReturn_t (WINAPI* PFN_nvmlPower)(nvmlDevice_t, unsigned int*);

struct Nvml {
    HMODULE dll;
    bool ready;
    PFN_nvmlInit init;
    PFN_nvmlShutdown shutdown;
    PFN_nvmlCount count;
    PFN_nvmlHandleByIndex handleByIndex;
    PFN_nvmlHandleByPciBusId handleByPciBusId;
    PFN_nvmlName name;
    PFN_nvmlPciInfo pciInfo;
    PFN_nvmlClock clock;
    PFN_nvmlUtil util;
    PFN_nvmlMem mem;
    PFN_nvmlTemp temp;
    PFN_nvmlPower power;

    Nvml()
    {
        ZeroMemory(this, sizeof(*this));
        dll = LoadLibraryA("nvml.dll");
        if (!dll) {
            debug_log("nvml.dll not present, no NVIDIA metrics");
            return;
        }
        init = (PFN_nvmlInit)GetProcAddress(dll, "nvmlInit_v2");
        shutdown = (PFN_nvmlShutdown)GetProcAddress(dll, "nvmlShutdown");
        count = (PFN_nvmlCount)GetProcAddress(dll, "nvmlDeviceGetCount_v2");
        handleByIndex = (PFN_nvmlHandleByIndex)GetProcAddress(dll, "nvmlDeviceGetHandleByIndex_v2");
        handleByPciBusId = (PFN_nvmlHandleByPciBusId)GetProcAddress(dll, "nvmlDeviceGetHandleByPciBusId_v2");
        name = (PFN_nvmlName)GetProcAddress(dll, "nvmlDeviceGetName");
        pciInfo = (PFN_nvmlPciInfo)GetProcAddress(dll, "nvmlDeviceGetPciInfo_v3");
        clock = (PFN_nvmlClock)GetProcAddress(dll, "nvmlDeviceGetClockInfo");
        util = (PFN_nvmlUtil)GetProcAddress(dll, "nvmlDeviceGetUtilizationRates");
        mem = (PFN_nvmlMem)GetProcAddress(dll, "nvmlDeviceGetMemoryInfo");
        temp = (PFN_nvmlTemp)GetProcAddress(dll, "nvmlDeviceGetTemperature");
        power = (PFN_nvmlPower)GetProcAddress(dll, "nvmlDeviceGetPowerUsage");

        if (!init || !shutdown || !count || !handleByIndex) {
            debug_log("nvml.dll present but required exports missing");
            FreeLibrary(dll);
            dll = NULL;
            return;
        }
        if (init() == 0) {
            ready = true;
            debug_log("nvml ready, pciInfo_v3=%s pciBusId=%s clockInfo=%s",
                      pciInfo ? "yes" : "no", handleByPciBusId ? "yes" : "no",
                      clock ? "yes" : "no");
        } else {
            debug_log("nvmlInit_v2 failed");
            FreeLibrary(dll);
            dll = NULL;
        }
    }

    ~Nvml()
    {
        if (ready) shutdown();
        if (dll) FreeLibrary(dll);
    }
};

void pci_bus_id_string(const hf_gpu* g, char* out, size_t n)
{
    snprintf(out, n, "%04x:%02x:%02x.%u", g->domain, g->bus, g->device, g->func);
}

/* Attach NVML metrics to the adapter whose PCI address matches. */
bool attach_nvml(Nvml& nvml, hf_gpu* g)
{
    nvmlDevice_t dev = NULL;
    char busId[32];
    char name[160] = {0};

    if (!nvml.ready) return false;
    if (g->vendor_id != 0x10DE) return false;
    if (g->func > 0xF) return false;   /* not a PCI device */

    if (nvml.handleByPciBusId) {
        pci_bus_id_string(g, busId, sizeof(busId));
        if (nvml.handleByPciBusId(busId, &dev) == 0 && dev) {
            debug_log("gpu '%s' matched via pciBusId %s", g->name, busId);
        } else {
            dev = NULL;
            debug_log("no NVML device for pciBusId %s", busId);
        }
    }

    if (!dev && nvml.pciInfo && nvml.count) {
        unsigned int total = 0;
        if (nvml.count(&total) == 0) {
            for (unsigned int i = 0; i < total; i++) {
                nvmlDevice_t candidate = NULL;
                nvmlPciInfoShim info;
                ZeroMemory(&info, sizeof(info));
                if (nvml.handleByIndex(i, &candidate) != 0 || !candidate) continue;
                if (nvml.pciInfo(candidate, &info) != 0) continue;
                unsigned int want = (g->device_id << 16) | g->vendor_id;
                if (info.pciDeviceId == want) {
                    dev = candidate;
                    debug_log("gpu '%s' matched via pciDeviceId %08X (index %u)",
                              g->name, want, i);
                    break;
                }
            }
        }
    }

    if (!dev) return false;

    g->have_nvml = 1;
    if (nvml.name)
        nvml.name(dev, name, sizeof(name));
    if (nvml.clock) {
        if (nvml.clock(dev, NVML_CLOCK_GRAPHICS, &g->core_mhz) != 0) g->core_mhz = 0;
        if (nvml.clock(dev, NVML_CLOCK_MEM, &g->mem_mhz) != 0) g->mem_mhz = 0;
    }
    if (nvml.util) {
        nvmlUtilization_t u;
        ZeroMemory(&u, sizeof(u));
        if (nvml.util(dev, &u) == 0) g->load_pct = u.gpu;
    }
    if (nvml.mem) {
        nvmlMemory_t m;
        ZeroMemory(&m, sizeof(m));
        if (nvml.mem(dev, &m) == 0) {
            g->vram_used = m.used;
            g->vram_total = m.total;
        }
    }
    if (nvml.temp) {
        unsigned int t = 0;
        if (nvml.temp(dev, NVML_TEMPERATURE_GPU, &t) == 0) g->temp_c = t;
    }
    if (nvml.power) {
        unsigned int mw = 0;
        if (nvml.power(dev, &mw) == 0) g->power_w = mw / 1000;
    }

    debug_log("nvml metrics: core=%u mem=%u load=%u%% vram=%llu/%llu temp=%u power=%uW",
              g->core_mhz, g->mem_mhz, g->load_pct,
              g->vram_used, g->vram_total, g->temp_c, g->power_w);
    return true;
}

} /* namespace */

void gpu_collect(hf_state* s)
{
    PFN_D3DKMTEnumAdapters2 pEnum = NULL;
    HMODULE gdi;
    HF_ADAPTERINFO adapters[HF_MAX_ENUM_ADAPTERS];
    HF_ENUMADAPTERS2 ea;
    Nvml nvml;
    LONG st;

    debug_begin("gpu");

    gdi = LoadLibraryA("gdi32.dll");
    if (!gdi) {
        debug_log("gdi32.dll load failed: %s", debug_win32(GetLastError()));
        s->gpu_count = 0;
        return;
    }
    pEnum = (PFN_D3DKMTEnumAdapters2)GetProcAddress(gdi, "D3DKMTEnumAdapters2");
    if (!pEnum) {
        debug_log("D3DKMTEnumAdapters2 missing");
        FreeLibrary(gdi);
        s->gpu_count = 0;
        return;
    }

    ZeroMemory(adapters, sizeof(adapters));
    ea.NumAdapters = HF_MAX_ENUM_ADAPTERS;
    ea.pAdapters = adapters;
    st = pEnum(&ea);
    if (st < 0) {
        debug_log("D3DKMTEnumAdapters2 failed: 0x%08lX", (unsigned long)st);
        FreeLibrary(gdi);
        s->gpu_count = 0;
        return;
    }
    debug_log("D3DKMTEnumAdapters2 -> %lu adapters", (unsigned long)ea.NumAdapters);

    for (ULONG i = 0; i < ea.NumAdapters && s->gpu_count < HF_MAX_GPUS; i++) {
        HF_ADAPTERADDRESS addr;
        HF_QUERY_DEVICE_IDS qids;
        HF_SEGMENTSIZEINFO seg;
        HF_ADAPTERTYPE atype;
        HF_ADAPTERREGISTRYINFO reg;
        hf_gpu* g = &s->gpus[s->gpu_count];

        ZeroMemory(g, sizeof(*g));
        ZeroMemory(&addr, sizeof(addr));
        ZeroMemory(&qids, sizeof(qids));
        ZeroMemory(&seg, sizeof(seg));
        ZeroMemory(&atype, sizeof(atype));
        ZeroMemory(&reg, sizeof(reg));

        if (!query(adapters[i], HF_KMTQAITYPE_ADAPTERREGISTRYINFO, &reg, sizeof(reg)))
            continue;
        if (!WideCharToMultiByte(CP_UTF8, 0, reg.AdapterString, -1,
                                 g->name, (int)sizeof(g->name), NULL, NULL))
            continue;
        if (g->name[0] == '\0') {
            debug_log("adapter %lu has no name, skipped", (unsigned long)i);
            continue;
        }

        if (query(adapters[i], HF_KMTQAITYPE_ADAPTERTYPE, &atype, sizeof(atype))) {
            g->software = atype.A.Bits.SoftwareDevice ? 1 : 0;
            if (g->software) {
                debug_log("adapter '%s' is a software device, skipped", g->name);
                continue;
            }
            g->integrated = atype.A.Bits.HybridIntegrated ? 1 : 0;
            if (!g->integrated && atype.A.Bits.HybridDiscrete)
                g->integrated = 0;
        }

        if (query(adapters[i], HF_KMTQAITYPE_ADAPTERADDRESS, &addr, sizeof(addr))) {
            if (addr.FunctionNumber <= 0xF) {
                g->domain = 0;
                g->bus = addr.BusNumber;
                g->device = addr.DeviceNumber;
                g->func = addr.FunctionNumber;
            } else {
                g->func = 0xFFFF;   /* not on PCI */
            }
        } else {
            g->func = 0xFFFF;
        }

        if (query(adapters[i], HF_KMTQAITYPE_PHYSICALADAPTERDEVICEIDS, &qids, sizeof(qids))) {
            g->vendor_id = qids.DeviceIds.VendorID;
            g->device_id = qids.DeviceIds.DeviceID;
        }

        if (query(adapters[i], HF_KMTQAITYPE_GETSEGMENTSIZE, &seg, sizeof(seg)))
            g->vram_total = seg.DedicatedVideoMemorySize;

        debug_log("adapter '%s' bus=%u:%u.%u ven=%04X dev=%04X %s dedicated=%llu",
                  g->name, g->bus, g->device, g->func, g->vendor_id, g->device_id,
                  g->integrated ? "integrated" : "discrete", g->vram_total);

        s->gpu_count++;
    }

    FreeLibrary(gdi);

    /* Discrete adapters first, matching the primary-card-first display. */
    for (int a = 0; a < s->gpu_count; a++) {
        for (int b = a + 1; b < s->gpu_count; b++) {
            if (s->gpus[a].integrated && !s->gpus[b].integrated) {
                hf_gpu tmp = s->gpus[a];
                s->gpus[a] = s->gpus[b];
                s->gpus[b] = tmp;
            }
        }
    }

    for (int i = 0; i < s->gpu_count; i++) {
        if (s->gpus[i].integrated) continue;
        attach_nvml(nvml, &s->gpus[i]);
    }

    for (int i = 0; i < s->gpu_count; i++)
        debug_log("final gpu[%d] '%s' %s nvml=%d", i, s->gpus[i].name,
                  s->gpus[i].integrated ? "integrated" : "discrete",
                  s->gpus[i].have_nvml);

    debug_end();
}